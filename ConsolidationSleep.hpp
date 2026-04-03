#ifndef ConsolidationSleep_hpp
#define ConsolidationSleep_hpp

#include <deque>
#include <map>
#include <set>

#include "SchedulingAlgorithm.hpp"

class ConsolidationSleep : public SchedulingAlgorithm {
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
    }

    void NewTask(Time_t now, TaskId_t task_id) override
    {
        (void)now;
        checks_since_new_task = 0;
        if (!TryPlaceTask(task_id)) {
            pending_tasks.push_back(task_id);
        }
    }

    void TaskComplete(Time_t now, TaskId_t task_id) override
    {
        (void)now;
        task_to_vm->erase(task_id);
        RefreshMachineStateSets();
        SleepIdleMachines();
    }

    void PeriodicCheck(Time_t now) override
    {
        (void)now;
        checks_since_new_task++;
        RefreshMachineStateSets();
        SleepIdleMachines();
        DrainPending();
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

    static constexpr unsigned kMinAwakePerCPU = 3;
    static constexpr unsigned kIdleChecksBeforeSleep = 10;
    static constexpr double kUrgentLoadCap = 0.70;
    static constexpr double kConsolidationCap = 0.60;

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
        // For lower-is-better scores, negative favors GPU; inverse for higher-is-better.
        if (lower_is_better) {
            return info.gpus ? -0.12 : 0.08;
        }
        return info.gpus ? 0.12 : -0.08;
    }

    double MachineLoadScoreConsolidation(const MachineInfo_t& info) const
    {
        double cpu_load = static_cast<double>(info.active_tasks) / static_cast<double>(info.num_cpus);
        double mem_load = static_cast<double>(info.memory_used) / static_cast<double>(info.memory_size);
        // Favor already-busy boxes to consolidate work.
        return 0.6 * cpu_load + 0.4 * mem_load;
    }

    double MachineLoadScoreSpread(const MachineInfo_t& info) const
    {
        double cpu_load = static_cast<double>(info.active_tasks) / static_cast<double>(info.num_cpus);
        double mem_load = static_cast<double>(info.memory_used) / static_cast<double>(info.memory_size);
        // Urgent tasks should avoid queues.
        return 0.5 * cpu_load + 0.5 * mem_load;
    }

    map<CPUType_t, unsigned> BuildAwakeCountByCPU() const
    {
        map<CPUType_t, unsigned> awake_count;
        const unsigned machine_count = Machine_GetTotal();
        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t machine = Machine_GetInfo(id);
            if (machine.s_state == S0) {
                awake_count[machine.cpu]++;
            }
        }
        return awake_count;
    }

    bool IsUrgentSLA(const TaskInfo_t& task) const
    {
        return task.required_sla == SLA0 || task.required_sla == SLA1;
    }

    void RefreshMachineStateSets()
    {
        const unsigned machine_count = Machine_GetTotal();
        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t machine = Machine_GetInfo(id);
            if (machine.s_state == S0) {
                waking_machines.erase(id);
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
        RefreshMachineStateSets();

        TaskInfo_t task = GetTaskInfo(task_id);
        const unsigned machine_count = Machine_GetTotal();
        const bool urgent = IsUrgentSLA(task);

        MachineId_t best_machine = static_cast<MachineId_t>(-1);
        MachineId_t least_loaded_machine = static_cast<MachineId_t>(-1);
        double least_loaded_score = 10.0;
        double best_score = urgent ? 10.0 : -1.0;

        // SLA-aware placement: urgent tasks spread, best-effort tasks consolidate.
        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t machine = Machine_GetInfo(id);
            if (!IsTaskFeasibleOnMachine(task, machine, true)) {
                continue;
            }

            if (urgent) {
                double score = MachineLoadScoreSpread(machine) + GPUPlacementBias(task, machine, true);
                if (score < best_score) {
                    best_score = score;
                    best_machine = id;
                }
                if (score < least_loaded_score) {
                    least_loaded_score = score;
                    least_loaded_machine = id;
                }
            } else {
                double spread_score = MachineLoadScoreSpread(machine) + GPUPlacementBias(task, machine, true);
                if (spread_score < least_loaded_score) {
                    least_loaded_score = spread_score;
                    least_loaded_machine = id;
                }
                double score = MachineLoadScoreConsolidation(machine) + GPUPlacementBias(task, machine, false);
                if (score < kConsolidationCap && score > best_score) {
                    best_score = score;
                    best_machine = id;
                }
            }
        }

        if (!urgent && best_machine == static_cast<MachineId_t>(-1) && least_loaded_machine != static_cast<MachineId_t>(-1)) {
            // If no safe consolidation target is found, fall back to spread to avoid queue blowups.
            best_machine = least_loaded_machine;
        }

        // If urgent placement would still be too loaded, prefer waking a cleaner machine.
        bool urgent_too_loaded = urgent && best_machine != static_cast<MachineId_t>(-1) && best_score > kUrgentLoadCap;

        if (best_machine != static_cast<MachineId_t>(-1)) {
            if (urgent_too_loaded) {
                // Skip immediate placement and try waking capacity below.
            } else {
                VMId_t vm_id = FindOrCreateVM(best_machine, task.required_vm);
                VM_AddTask(vm_id, task_id, PriorityForTask(task));
                (*task_to_vm)[task_id] = vm_id;
                idle_streak[best_machine] = 0;
                sleeping_machines.erase(best_machine);
                return true;
            }
        }

        // If no awake feasible machine exists (or urgent one is too loaded), wake one compatible machine.
        map<CPUType_t, unsigned> awake_count = BuildAwakeCountByCPU();
        MachineId_t wake_candidate = static_cast<MachineId_t>(-1);
        double wake_candidate_score = 10.0;
        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t machine = Machine_GetInfo(id);
            if (!IsTaskFeasibleOnMachine(task, machine, false)) {
                continue;
            }
            if (machine.s_state == S0) {
                continue;
            }
            if (waking_machines.count(id) != 0) {
                continue;
            }
            double score = MachineLoadScoreSpread(machine) + GPUPlacementBias(task, machine, true);
            if (score < wake_candidate_score) {
                wake_candidate_score = score;
                wake_candidate = id;
            }
        }

        if (wake_candidate != static_cast<MachineId_t>(-1)) {
            MachineInfo_t machine = Machine_GetInfo(wake_candidate);
            waking_machines.insert(wake_candidate);
            sleeping_machines.erase(wake_candidate);
            awake_count[machine.cpu]++;
            Machine_SetState(wake_candidate, S0);
        }

        // For urgent tasks, as a fallback, place on least-loaded awake machine if available.
        if (urgent && best_machine != static_cast<MachineId_t>(-1) && urgent_too_loaded) {
            VMId_t vm_id = FindOrCreateVM(best_machine, task.required_vm);
            VM_AddTask(vm_id, task_id, PriorityForTask(task));
            (*task_to_vm)[task_id] = vm_id;
            idle_streak[best_machine] = 0;
            sleeping_machines.erase(best_machine);
            return true;
        }

        return false;
    }

    void SleepIdleMachines()
    {
        // Avoid sleep-state thrashing while arrivals are still active.
        if (!pending_tasks.empty() || checks_since_new_task < (kIdleChecksBeforeSleep * 6)) {
            return;
        }

        map<CPUType_t, unsigned> awake_count = BuildAwakeCountByCPU();
        const unsigned machine_count = Machine_GetTotal();
        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t machine = Machine_GetInfo(id);
            if (machine.s_state != S0) {
                idle_streak[id] = 0;
                continue;
            }
            if (machine.active_tasks != 0 || machine.active_vms == 0) {
                idle_streak[id] = 0;
                continue;
            }
            if (waking_machines.count(id) != 0) {
                continue;
            }
            if (awake_count[machine.cpu] <= kMinAwakePerCPU) {
                idle_streak[id] = 0;
                continue;
            }

            idle_streak[id]++;
            if (sleeping_machines.count(id) == 0) {
                if (idle_streak[id] >= kIdleChecksBeforeSleep) {
                    // Use S0i1 for low latency while reducing idle burn.
                    Machine_SetState(id, S0i1);
                    sleeping_machines.insert(id);
                    if (awake_count[machine.cpu] > 0) {
                        awake_count[machine.cpu]--;
                    }
                }
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
            TaskId_t task_id = pending_tasks.front();
            pending_tasks.pop_front();
            if (!TryPlaceTask(task_id)) {
                still_pending.push_back(task_id);
            }
        }
        pending_tasks.swap(still_pending);
    }
};

#endif
