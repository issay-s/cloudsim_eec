// RoundRobin.hpp
#include "SchedulingAlgorithm.hpp"

class Greedy : public SchedulingAlgorithm {
public:
    void Init(vector<VMId_t>& vms, vector<MachineId_t>& machines) override {
        this->vms = vms;
        this->machines = machines;
    }
    void HandleNewTask(Time_t time, TaskId_t task_id)
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

        VM_AddTask()
    }
    void TaskComplete(Time_t now, TaskId_t task_id) override { /* ... */ }
    void PeriodicCheck(Time_t now) override { /* ... */ }
    void MigrationComplete(Time_t time, VMId_t vm_id) override { /* ... */ }
    void Shutdown(Time_t time) override {
        for (auto& vm : vms) VM_Shutdown(vm);
    }

private:
    vector<VMId_t> vms;
    vector<MachineId_t> machines;
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