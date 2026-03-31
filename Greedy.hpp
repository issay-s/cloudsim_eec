// Greedy.hpp
#include "SchedulingAlgorithm.hpp"


class Greedy : public SchedulingAlgorithm {
public:

    map<MachineId_t, vector<VMId_t>>* machine_to_vms; 
    map<VMId_t, MachineId_t>* vm_to_machine;
    map<TaskId_t, VMId_t>* task_to_vm;

    void Init(
              map<MachineId_t, vector<VMId_t>> &m2v,
              map<VMId_t, MachineId_t> &v2m,
              map<TaskId_t, VMId_t> &t2v) override
    {
        machine_to_vms = &m2v;
        vm_to_machine = &v2m;
        task_to_vm = &t2v;
    }
    void NewTask(Time_t time, TaskId_t task_id)
    {
        TaskInfo_t task = GetTaskInfo(task_id);

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
        if (best == (MachineId_t)-1)
        {
            return; // TODO no machine found — need to handle this (wake a machine, queue task, etc.)
        }

        // Get a VM on the best machine (or create one if the right type doesn't exist)
        VMId_t target_vm = findOrCreateVM(best, RequiredVMType(task_id)); 

        // Determine priority based on SLA
        Priority_t priority;
        if (task.required_sla == SLA0)
            priority = HIGH_PRIORITY;
        else if (task.required_sla == SLA1)
            priority = MID_PRIORITY;
        else
            priority = LOW_PRIORITY;

        VM_AddTask(target_vm, task_id, priority);
        (*task_to_vm)[task_id] = target_vm; // update the map!
    }
    void TaskComplete(Time_t now, TaskId_t task_id) override { /* ... */ }
    void PeriodicCheck(Time_t now) override { /* ... */ }
    void MigrationComplete(Time_t time, VMId_t vm_id) override { /* ... */ }
    void Shutdown(Time_t time) override {
        task_to_vm->clear();    
    }

private:
    VMId_t findOrCreateVM(MachineId_t mid, VMType_t required_vm)
    {
        // Look for an existing VM of the right type on this machine
        for (VMId_t vm : (*machine_to_vms)[mid])
        {
            VMInfo_t info = VM_GetInfo(vm);
            if (info.vm_type == required_vm)
                return vm;
        }

        // None found — create one dynamically
        MachineInfo_t m = Machine_GetInfo(mid);
        VMId_t new_vm = VM_Create(required_vm, m.cpu);
        VM_Attach(new_vm, mid);

        (*machine_to_vms)[mid].push_back(new_vm);
        (*vm_to_machine)[new_vm] = mid;
        
        (*machine_to_vms)[mid].push_back(new_vm);

        return new_vm;
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