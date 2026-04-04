#ifndef AdaptiveHybrid_hpp
#define AdaptiveHybrid_hpp

#include <deque>
#include <map>
#include <set>

#include "SchedulingAlgorithm.hpp"

// this file was written in collaboration with Cursor AI.
// nearly everything in it was iterated over by both Lance and the AI,
// with the help of Issay

// Adaptive hybrid policy:
// - SLA mode: spread urgent work to reduce queueing delay.
// - Energy mode: consolidate best-effort work under a utilization cap.
// - Hysteresis-based sleep/wake decisions with a warm-capacity floor.
class AdaptiveHybrid : public SchedulingAlgorithm {
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
        TaskInfo_t task = GetTaskInfo(task_id);
        if (IsUrgentSLA(task)) {
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

    static constexpr double kUrgentUpperUtil = 0.78;
    static constexpr double kEnergyUpperUtil = 0.92;
    static constexpr double kSleepLowerUtil = 0.20;
    static constexpr unsigned kIdleChecksToS0i1 = 14;
    static constexpr unsigned kIdleChecksToS1 = 42;
    static constexpr unsigned kMinAwakePerCPU = 4;
    static constexpr unsigned kUrgentCooldownWindow = 10;

    bool InSlaMode() const
    {
        return !pending_tasks.empty() || urgent_cooldown_checks > 0;
    }

    bool IsUrgentSLA(const TaskInfo_t& task) const
    {
        return task.required_sla == SLA0 || task.required_sla == SLA1;
    }

    Priority_t PriorityForTask(const TaskInfo_t& task) const
    {
        if (task.required_sla == SLA0 || task.required_sla == SLA1) {
            return HIGH_PRIORITY;
        }
        return LOW_PRIORITY;
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
                // Sleep transition has landed; allow future sleep requests.
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
        MachineInfo_t m = Machine_GetInfo(machine_id);
        VMId_t vm = VM_Create(required_vm, m.cpu);
        VM_Attach(vm, machine_id);
        (*machine_to_vms)[machine_id].push_back(vm);
        (*vm_to_machine)[vm] = machine_id;
        return vm;
    }

    bool TryPlaceTask(TaskId_t task_id)
    {
        TaskInfo_t task = GetTaskInfo(task_id);
        const bool urgent = IsUrgentSLA(task);
        const bool sla_mode = InSlaMode();
        const unsigned machine_count = Machine_GetTotal();

        MachineId_t best = static_cast<MachineId_t>(-1);
        MachineId_t fallback = static_cast<MachineId_t>(-1);
        double best_metric = (urgent || sla_mode) ? 10.0 : -1.0;
        double fallback_metric = 10.0;

        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t m = Machine_GetInfo(id);
            if (!IsTaskFeasibleOnMachine(task, m, true)) {
                continue;
            }

            double util = UtilScore(m);
            double fallback_score = util + GPUPlacementBias(task, m, true);
            if (fallback_score < fallback_metric) {
                fallback_metric = fallback_score;
                fallback = id;
            }

            if (urgent || sla_mode) {
                double urgent_score = util + GPUPlacementBias(task, m, true);
                if (util <= kUrgentUpperUtil && urgent_score < best_metric) {
                    best_metric = urgent_score;
                    best = id;
                }
            } else {
                double be_score = util + GPUPlacementBias(task, m, false);
                if (util <= kEnergyUpperUtil && be_score > best_metric) {
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

        // Wake a compatible non-awake machine.
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
        // Keep latency low while arrivals are active, queue is non-empty,
        // or we are still in the recent-urgency cooldown window.
        if (!pending_tasks.empty() || checks_since_new_task < (kIdleChecksToS0i1 * 4) || urgent_cooldown_checks > 0) {
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
            if (UtilScore(m) > kSleepLowerUtil) {
                idle_streak[id] = 0;
                continue;
            }

            idle_streak[id]++;
            if (idle_streak[id] >= kIdleChecksToS1 && urgent_cooldown_checks == 0) {
                sleeping_machines.insert(id);
                Machine_SetState(id, S1);
                if (awake_count[m.cpu] > 0) {
                    awake_count[m.cpu]--;
                }
            } else if (idle_streak[id] >= kIdleChecksToS0i1) {
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
