// SchedulingAlgorithm.hpp
#ifndef SchedulingAlgorithm_hpp
#define SchedulingAlgorithm_hpp

#include "Interfaces.h"
#include <vector>

class SchedulingAlgorithm {
public:
    virtual ~SchedulingAlgorithm() {}
    virtual void Init(vector<VMId_t>& vms, vector<MachineId_t>& machines) = 0;
    virtual void NewTask(Time_t now, TaskId_t task_id) = 0;
    virtual void TaskComplete(Time_t now, TaskId_t task_id) = 0;
    virtual void PeriodicCheck(Time_t now) = 0;
    virtual void MigrationComplete(Time_t time, VMId_t vm_id) = 0;
    virtual void Shutdown(Time_t time) = 0;
};

#endif
