//
//  Scheduler.hpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#ifndef Scheduler_hpp
#define Scheduler_hpp

#include <vector>
#include <map>

#include "Interfaces.h"
#include "SchedulingAlgorithm.hpp"

class Scheduler {
public:
    Scheduler()                 {}
    void Init();
    void MigrationComplete(Time_t time, VMId_t vm_id);
    void NewTask(Time_t now, TaskId_t task_id);
    void PeriodicCheck(Time_t now);
    void Shutdown(Time_t now);
    void TaskComplete(Time_t now, TaskId_t task_id);
private:
    SchedulingAlgorithm* algo;
    vector<VMId_t> vms;
    vector<MachineId_t> machines;

    map<MachineId_t, vector<VMId_t>> machine_to_vms;
    map<VMId_t, MachineId_t> vm_to_machine;
    map<TaskId_t, VMId_t> task_to_vm;
};



#endif /* Scheduler_hpp */
