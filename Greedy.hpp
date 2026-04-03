#ifndef Greedy_hpp
#define Greedy_hpp

#include <deque>
#include <map>
#include <set>

#include "SchedulingAlgorithm.hpp"

// overall structure written/scaffolded by Cursor AI. Fine tuning and algo details written by hand.

class Greedy : public SchedulingAlgorithm {
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
    }

    void NewTask(Time_t now, TaskId_t task_id) override
    {
        (void)now;
        if (!TryPlaceTask(task_id)) {
            pending_tasks.push_back(task_id);
        }
    }

    void TaskComplete(Time_t now, TaskId_t task_id) override
    {
        (void)now;
        task_to_vm->erase(task_id);
    }

    void PeriodicCheck(Time_t now) override
    {
        (void)now;
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
    }

private:
    map<MachineId_t, vector<VMId_t>>* machine_to_vms = nullptr;
    map<VMId_t, MachineId_t>* vm_to_machine = nullptr;
    map<TaskId_t, VMId_t>* task_to_vm = nullptr;
    deque<TaskId_t> pending_tasks;
    set<MachineId_t> waking_machines;

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

    double MachineLoadScore(const MachineInfo_t& info) const
    {
        double cpu_load = static_cast<double>(info.active_tasks) / static_cast<double>(info.num_cpus);
        double mem_load = static_cast<double>(info.memory_used) / static_cast<double>(info.memory_size);
        return 0.5 * cpu_load + 0.5 * mem_load;
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
        const unsigned machine_count = Machine_GetTotal();
        MachineId_t best = -1;
        double bestUtil = 2.0; // start above max possible

        for (MachineId_t mid = 0; mid < Machine_GetTotal(); mid++)
        {
            MachineInfo_t m = Machine_GetInfo(mid);

            // Hard feasibility filters
            if (m.s_state != S0)
                continue; // must be awake
            if (m.cpu != task.required_cpu)
                continue; // CPU compatibility
            if (m.memory_size - m.memory_used < task.required_memory)
                continue; // enough RAM

            double util = getMachineUtilization(mid);
            if (util < bestUtil)
            {
                bestUtil = util;
                best = mid;
            }
        }

        if (best != static_cast<MachineId_t>(-1)) {
            VMId_t vm_id = FindOrCreateVM(best, task.required_vm);
            VM_AddTask(vm_id, task_id, PriorityForTask(task));
            (*task_to_vm)[task_id] = vm_id;
            return true;
        }

        for (MachineId_t id = 0; id < machine_count; id++) {
            MachineInfo_t machine = Machine_GetInfo(id);
            if (!IsTaskFeasibleOnMachine(task, machine, false)) {
                continue;
            }
            if (machine.s_state == S0) {
                continue;
            }
            if (waking_machines.count(id) == 0) {
                waking_machines.insert(id);
                Machine_SetState(id, S0);
            }
            break;
        }

        return false;
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
    double getMachineUtilization(MachineId_t mid)
    {
        MachineInfo_t info = Machine_GetInfo(mid);

        // Memory utilization (hard constraint — overcommit causes serious slowdown)
        double memUtil = (double)info.memory_used / info.memory_size;

        // CPU utilization (active tasks vs total cores)
        double cpuUtil = (double)info.active_tasks / info.num_cpus;

        // Weight memory higher — overcommit is catastrophic per the spec
        return 0.4 * cpuUtil + 0.6 * memUtil;
    }
    double getTaskLoad(TaskId_t tid, MachineId_t mid)
    {
        TaskInfo_t task = GetTaskInfo(tid);
        MachineInfo_t machine = Machine_GetInfo(mid);

        // Normalized memory pressure — how much of the machine's RAM does this task eat?
        double memPressure = (double)task.required_memory / machine.memory_size;

        // Normalized CPU pressure — one task occupies roughly 1/num_cpus of compute
        double cpuPressure = 1.0 / machine.num_cpus;

        return 0.4 * cpuPressure + 0.6 * memPressure;
    }
};

#endif