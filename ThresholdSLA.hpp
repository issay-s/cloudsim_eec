#ifndef ThresholdSLA_hpp
#define ThresholdSLA_hpp

#include <deque>
#include <map>
#include <set>

#include "SchedulingAlgorithm.hpp"

// AI attribution: policy development included iterative assistance from Cursor AI (GPT-5.3 Codex).
// Literature-inspired threshold policy:
// - keep machine utilization between lower/upper bounds,
// - prioritize SLA-sensitive tasks on lower-load machines,
// - consolidate best-effort tasks without crossing upper bound.
class ThresholdSLA : public SchedulingAlgorithm {
public:
    void Init(
        map<MachineId_t, vector<VMId_t>>& m2v,
        map<VMId_t, MachineId_t>& v2m,
        map<TaskId_t, VMId_t>& t2v
    ) override
    {
        machine_to_vms = &m2v;
        vm_to_machine = &v2m;
        task_to_vm = &t2v;
        pending_tasks.clear();
        waking_machines.clear();
        sleeping_machines.clear();
        idle_streak.clear();
        checks_since_new_task = 0;
        urgent_cooldown_checks = 0;
    }

    void NewTask(Time_t now, TaskId_t task_id) override
    {
        (void)now;
        checks_since_new_task = 0;
        if (IsUrgentSLA(GetTaskInfo(task_id))) {
            urgent_cooldown_checks = kUrgentCooldownWindow;
        }
        if (!TryPlaceTask(task_id)) {
            pending_tasks.push_back(task_id);
        }
    }

    void TaskComplete(Time_t now, TaskId_t task_id) override
    {
        (void)now;
        task_to_vm->erase(task_id);
        RefreshWakingSet();
    }

    void PeriodicCheck(Time_t now) override
    {
        (void)now;
        checks_since_new_task++;
        if (urgent_cooldown_checks > 0) {
            urgent_cooldown_checks--;
        }
        RefreshWakingSet();
        DrainPending();
        SleepIdleMachines();
    }

    void MigrationComplete(Time_t time, VMId_t vm_id) override
    {
        (void)time;
        (void)vm_id;
    }

    void Shutdown(Time_t time) override
    {
        (void)time;
        pending_tasks.clear();
        waking_machines.clear();
        sleeping_machines.clear();
        idle_streak.clear();
        checks_since_new_task = 0;
        urgent_cooldown_checks = 0;
    }

private:
    map<MachineId_t, vector<VMId_t>>* machine_to_vms = nullptr;
    map<VMId_t, MachineId_t>* vm_to_machine = nullptr;
    map<TaskId_t, VMId_t>* task_to_vm = nullptr;
    deque<TaskId_t> pending_tasks;
    set<MachineId_t> waking_machines;
    set<MachineId_t> sleeping_machines;
    map<MachineId_t, unsigned> idle_streak;
    unsigned checks_since_new_task = 0;
    unsigned urgent_cooldown_checks = 0;

    static constexpr double kUpperUtilUrgent = 0.75;
    static constexpr double kUpperUtilBestEffort = 0.88;
    static constexpr double kLowerUtil = 0.18;
    static constexpr unsigned kIdleChecksBeforeSleep = 10;
    static constexpr unsigned kDeepSleepChecks = 30;
    static constexpr unsigned kMinAwakePerCPU = 3;
    static constexpr unsigned kUrgentCooldownWindow = 4;

    Priority_t PriorityForTask(const TaskInfo_t& task) const
    {
        if (task.required_sla == SLA0) {
            return HIGH_PRIORITY;
        }
        if (task.required_sla == SLA1) {
            return MID_PRIORITY;
        }
        return LOW_PRIORITY;
    }

    bool IsUrgentSLA(const TaskInfo_t& task) const
    {
        return task.required_sla == SLA0 || task.required_sla == SLA1;
    }

    bool IsTaskFeasibleOnMachine(const TaskInfo_t& task, const MachineInfo_t& info, bool require_awake) const
    {
        if (task.required_cpu != info.cpu) {
            return false;
        }
        if (task.required_memory > (info.memory_size - info.memory_used)) {
            return false;
        }
        if (require_awake && info.s_state != S0) {
            return false;
        }
        return true;
    }

    double GPUPlacementBias(const TaskInfo_t& task, const MachineInfo_t& info, bool lower_is_better) const
    {
        if (!task.gpu_capable) {
            return 0.0;
        }
        if (lower_is_better) {
            return info.gpus ? -0.12 : 0.08;
        }
        return info.gpus ? 0.12 : -0.08;
    }

    double UtilScore(const MachineInfo_t& info) const
    {
        double cpu = static_cast<double>(info.active_tasks) / static_cast<double>(info.num_cpus);
        double mem = static_cast<double>(info.memory_used) / static_cast<double>(info.memory_size);
        return 0.5 * cpu + 0.5 * mem;
    }

    map<CPUType_t, unsigned> BuildAwakeCountByCPU() const
    {
        map<CPUType_t, unsigned> awake_count;
        const unsigned machine_count = Machine_GetTotal();
        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t m = Machine_GetInfo(id);
            if (m.s_state == S0) {
                awake_count[m.cpu]++;
            }
        }
        return awake_count;
    }

    void RefreshWakingSet()
    {
        const unsigned machine_count = Machine_GetTotal();
        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t m = Machine_GetInfo(id);
            if (m.s_state == S0) {
                waking_machines.erase(id);
            } else {
                sleeping_machines.erase(id);
            }
        }
    }

    VMId_t FindOrCreateVM(MachineId_t machine_id, VMType_t required_vm)
    {
        for (VMId_t vm_id : (*machine_to_vms)[machine_id]) {
            VMInfo_t info = VM_GetInfo(vm_id);
            if (info.vm_type == required_vm) {
                return vm_id;
            }
        }
        MachineInfo_t machine = Machine_GetInfo(machine_id);
        VMId_t vm_id = VM_Create(required_vm, machine.cpu);
        VM_Attach(vm_id, machine_id);
        (*machine_to_vms)[machine_id].push_back(vm_id);
        (*vm_to_machine)[vm_id] = machine_id;
        return vm_id;
    }

    bool TryPlaceTask(TaskId_t task_id)
    {
        TaskInfo_t task = GetTaskInfo(task_id);
        const bool urgent = IsUrgentSLA(task);
        const unsigned machine_count = Machine_GetTotal();

        MachineId_t best = static_cast<MachineId_t>(-1);
        MachineId_t fallback = static_cast<MachineId_t>(-1);
        double best_metric = urgent ? 10.0 : -1.0;
        double fallback_metric = 10.0;

        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t m = Machine_GetInfo(id);
            if (!IsTaskFeasibleOnMachine(task, m, true)) {
                continue;
            }
            if (sleeping_machines.count(id) != 0) {
                continue;
            }
            double util = UtilScore(m);

            double fallback_score = util + GPUPlacementBias(task, m, true);
            if (fallback_score < fallback_metric) {
                fallback_metric = fallback_score;
                fallback = id;
            }

            if (urgent) {
                // Urgent tasks prefer lower queueing delay.
                double urgent_score = util + GPUPlacementBias(task, m, true);
                if (util <= kUpperUtilUrgent && urgent_score < best_metric) {
                    best_metric = urgent_score;
                    best = id;
                }
            } else {
                // Best-effort tasks prefer consolidation under the cap.
                double be_score = util + GPUPlacementBias(task, m, false);
                if (util <= kUpperUtilBestEffort && be_score > best_metric) {
                    best_metric = be_score;
                    best = id;
                }
            }
        }

        if (best == static_cast<MachineId_t>(-1)) {
            best = fallback;
        }

        if (best != static_cast<MachineId_t>(-1)) {
            VMId_t vm = FindOrCreateVM(best, task.required_vm);
            VM_AddTask(vm, task_id, PriorityForTask(task));
            (*task_to_vm)[task_id] = vm;
            idle_streak[best] = 0;
            return true;
        }

        // No awake feasible machine; wake one compatible machine.
        MachineId_t wake_candidate = static_cast<MachineId_t>(-1);
        double wake_metric = 10.0;
        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t m = Machine_GetInfo(id);
            if (!IsTaskFeasibleOnMachine(task, m, false)) {
                continue;
            }
            if (m.s_state == S0 || waking_machines.count(id) != 0) {
                continue;
            }
            double util = UtilScore(m);
            double wake_score = util + GPUPlacementBias(task, m, true);
            if (wake_score < wake_metric) {
                wake_metric = wake_score;
                wake_candidate = id;
            }
        }
        if (wake_candidate != static_cast<MachineId_t>(-1)) {
            waking_machines.insert(wake_candidate);
            Machine_SetState(wake_candidate, S0);
        }
        return false;
    }

    void SleepIdleMachines()
    {
        // Let arrivals settle before sleeping machines.
        if (!pending_tasks.empty() || checks_since_new_task < (kIdleChecksBeforeSleep * 4) || urgent_cooldown_checks > 0) {
            return;
        }

        map<CPUType_t, unsigned> awake_count = BuildAwakeCountByCPU();
        const unsigned machine_count = Machine_GetTotal();

        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t m = Machine_GetInfo(id);
            if (m.s_state != S0 || waking_machines.count(id) != 0) {
                idle_streak[id] = 0;
                continue;
            }
            if (sleeping_machines.count(id) != 0) {
                continue;
            }
            if (m.active_tasks != 0) {
                idle_streak[id] = 0;
                continue;
            }
            if (awake_count[m.cpu] <= kMinAwakePerCPU) {
                idle_streak[id] = 0;
                continue;
            }

            // Sleep only clearly underutilized / idle machines after hysteresis.
            double util = UtilScore(m);
            if (util > kLowerUtil) {
                idle_streak[id] = 0;
                continue;
            }

            idle_streak[id]++;
            if (idle_streak[id] >= kDeepSleepChecks && urgent_cooldown_checks == 0) {
                sleeping_machines.insert(id);
                Machine_SetState(id, S1);
                if (awake_count[m.cpu] > 0) {
                    awake_count[m.cpu]--;
                }
            } else if (idle_streak[id] >= kIdleChecksBeforeSleep) {
                sleeping_machines.insert(id);
                Machine_SetState(id, S0i1);
            }
        }
    }

    void DrainPending()
    {
        if (pending_tasks.empty()) {
            return;
        }
        deque<TaskId_t> still_pending;
        while (!pending_tasks.empty()) {
            TaskId_t tid = pending_tasks.front();
            pending_tasks.pop_front();
            if (!TryPlaceTask(tid)) {
                still_pending.push_back(tid);
            }
        }
        pending_tasks.swap(still_pending);
    }
};

#endif
