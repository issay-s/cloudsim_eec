//
//  Scheduler.cpp
//  CloudSim
//
//  Created by ELMOOTAZBELLAH ELNOZAHY on 10/20/24.
//

#include <map>

#include "SchedulingAlgorithm.hpp"
#include "Scheduler.hpp"
#include "ConservativeSpread.hpp"
#include "ConsolidationSleep.hpp"
#include "ThresholdSLA.hpp"
#include "AdaptiveHybrid.hpp"
#include "Greedy.hpp"
#include <stdio.h>

static unsigned active_machines;
static unsigned sla_warning_count = 0;
static unsigned memory_warning_count = 0;
static unsigned pstate_pressure_cooldown = 0;

#ifndef SCHED_ALGO
#define SCHED_ALGO 2
#endif

#ifndef ENABLE_PSTATE_TUNING
#define ENABLE_PSTATE_TUNING 0
#endif

void Scheduler::Init() {
    // Find the parameters of the clusters
    // Get the total number of machines
    // For each machine:
    //      Get the type of the machine
    //      Get the memory of the machine
    //      Get the number of CPUs
    //      Get if there is a GPU or not
    active_machines = Machine_GetTotal();
    SimOutput("Scheduler::Init(): Total number of machines is " + to_string(Machine_GetTotal()), 3);
    SimOutput("Scheduler::Init(): Initializing scheduler", 1);

    // initialize our machine
    MachineId_t curr_machine;
    MachineInfo_t curr_machine_info;
    for(unsigned i = 0; i < active_machines; i++) {
        // physical machine stuff
        curr_machine = MachineId_t(i);
        machines.push_back(curr_machine);
        curr_machine_info = Machine_GetInfo(curr_machine); 

        VMId_t vm_id = VM_Create(LINUX, curr_machine_info.cpu);
        vms.push_back(vm_id);

        VM_Attach(vms[i], machines[i]);

        // Wire up the maps
        machine_to_vms[curr_machine].push_back(vm_id);
        vm_to_machine[vm_id] = curr_machine;
    }    

#if SCHED_ALGO == 2
    algo = new ConsolidationSleep();
    SimOutput("Scheduler::Init(): Using ConsolidationSleep policy", 1);
#elif SCHED_ALGO == 3
    algo = new ThresholdSLA();
    SimOutput("Scheduler::Init(): Using ThresholdSLA policy", 1);
#elif SCHED_ALGO == 4
    algo = new Greedy();
    SimOutput("Scheduler::Init(): Using Greedy policy", 1);
#elif SCHED_ALGO == 5
    algo = new AdaptiveHybrid();
    SimOutput("Scheduler::Init(): Using AdaptiveHybrid policy", 1);
#else
    algo = new ConservativeSpread();
    SimOutput("Scheduler::Init(): Using ConservativeSpread policy", 1);
#endif
    algo->Init(machine_to_vms, vm_to_machine, task_to_vm);

    SimOutput("Scheduler::Init(): VM ids are " + to_string(vms[0]) + " ahd " + to_string(vms[1]), 3);
}

void Scheduler::MigrationComplete(Time_t time, VMId_t vm_id) {
    algo->MigrationComplete(time, vm_id);
}

void Scheduler::NewTask(Time_t now, TaskId_t task_id) {
    // Get the task parameters
    //  IsGPUCapable(task_id);
    //  GetMemory(task_id);
    //  RequiredVMType(task_id);
    //  RequiredSLA(task_id);
    //  RequiredCPUType(task_id);
    // Decide to attach the task to an existing VM, 
    //      vm.AddTask(taskid, Priority_T priority); or
    // Create a new VM, attach the VM to a machine
    //      VM vm(type of the VM)
    //      vm.Attach(machine_id);
    //      vm.AddTask(taskid, Priority_t priority) or
    // Turn on a machine, create a new VM, attach it to the VM, then add the task
    //
    // Turn on a machine, migrate an existing VM from a loaded machine....
    //
    // Other possibilities as desired
    algo->NewTask(now, task_id);
}

void Scheduler::PeriodicCheck(Time_t now) {
    // This method should be called from SchedulerCheck()
    // SchedulerCheck is called periodically by the simulator to allow you to monitor, make decisions, adjustments, etc.
    // Unlike the other invocations of the scheduler, this one doesn't report any specific event
    // Recommendation: Take advantage of this function to do some monitoring and adjustments as necessary
    algo->PeriodicCheck(now);
}

void Scheduler::Shutdown(Time_t time) {
    // Do your final reporting and bookkeeping here.
    // Report about the total energy consumed
    // Report about the SLA compliance
    // Shutdown everything to be tidy :-)
    // Some policies can leave machines in sleep states at simulation end.
    // VM_Shutdown can throw in that condition, and metrics are already reported.
    // Keep shutdown side-effect free for stable experiment runs.
    SimOutput("SimulationComplete(): Finished!", 4);
    SimOutput("SimulationComplete(): Time is " + to_string(time), 4);
}

void Scheduler::TaskComplete(Time_t now, TaskId_t task_id) {
    // Do any bookkeeping necessary for the data structures
    // Decide if a machine is to be turned off, slowed down, or VMs to be migrated according to your policy
    // This is an opportunity to make any adjustments to optimize performance/energy
    SimOutput("Scheduler::TaskComplete(): Task " + to_string(task_id) + " is complete at " + to_string(now), 4);
    algo->TaskComplete(now, task_id);
}

// Public interface below

static Scheduler Scheduler;

void InitScheduler() {
    SimOutput("InitScheduler(): Initializing scheduler", 4);
    Scheduler.Init();
}

void HandleNewTask(Time_t time, TaskId_t task_id) {
    SimOutput("HandleNewTask(): Received new task " + to_string(task_id) + " at time " + to_string(time), 4);
    Scheduler.NewTask(time, task_id);
}

void HandleTaskCompletion(Time_t time, TaskId_t task_id) {
    SimOutput("HandleTaskCompletion(): Task " + to_string(task_id) + " completed at time " + to_string(time), 4);
    Scheduler.TaskComplete(time, task_id);
}

void MemoryWarning(Time_t time, MachineId_t machine_id) {
    // The simulator is alerting you that machine identified by machine_id is overcommitted
    SimOutput("MemoryWarning(): Overflow at " + to_string(machine_id) + " was detected at time " + to_string(time), 0);
    memory_warning_count++;
    pstate_pressure_cooldown = 8;

    // Emergency response: maximize local compute speed, then wake one compatible peer.
    MachineInfo_t hot = Machine_GetInfo(machine_id);
    if (hot.s_state == S0) {
        Machine_SetCorePerformance(machine_id, 0, P0);
    }

    for (MachineId_t mid = 0; mid < Machine_GetTotal(); mid++) {
        if (mid == machine_id) {
            continue;
        }
        MachineInfo_t m = Machine_GetInfo(mid);
        if (m.cpu != hot.cpu) {
            continue;
        }
        if (m.s_state == S0) {
            continue;
        }
        Machine_SetState(mid, S0);
        break;
    }
}

void MigrationDone(Time_t time, VMId_t vm_id) {
    // The function is called on to alert you that migration is complete
    SimOutput("MigrationDone(): Migration of VM " + to_string(vm_id) + " was completed at time " + to_string(time), 4);
    Scheduler.MigrationComplete(time, vm_id);
}

void SchedulerCheck(Time_t time) {
    // This function is called periodically by the simulator, no specific event
    SimOutput("SchedulerCheck(): SchedulerCheck() called at " + to_string(time), 4);
    Scheduler.PeriodicCheck(time);
    if (pstate_pressure_cooldown > 0) {
        pstate_pressure_cooldown--;
    }

    // Optional dynamic P-state tuning.
#if ENABLE_PSTATE_TUNING
    // Keep low load machines in lower-performance states to reduce dynamic power.
    for (MachineId_t mid = 0; mid < Machine_GetTotal(); mid++) {
        MachineInfo_t m = Machine_GetInfo(mid);
        if (m.s_state != S0) {
            continue;
        }

        CPUPerformance_t target = P0;
        if (m.active_tasks == 0) {
            // Only down-clock truly idle machines.
            target = (pstate_pressure_cooldown > 0) ? P1 : P2;
        }

        if (m.p_state != target) {
            Machine_SetCorePerformance(mid, 0, target);
        }
    }
#endif
}

void SimulationComplete(Time_t time) {
    // This function is called before the simulation terminates Add whatever you feel like.
    cout << "SLA violation report" << endl;
    cout << "SLA0: " << GetSLAReport(SLA0) << "%" << endl;
    cout << "SLA1: " << GetSLAReport(SLA1) << "%" << endl;
    cout << "SLA2: " << GetSLAReport(SLA2) << "%" << endl;     // SLA3 do not have SLA violation issues
    cout << "Total Energy " << Machine_GetClusterEnergy() << "KW-Hour" << endl;
    cout << "Simulation run finished in " << double(time)/1000000 << " seconds" << endl;
    SimOutput("SimulationComplete(): Simulation finished at time " + to_string(time), 4);
    
    Scheduler.Shutdown(time);
}

void SLAWarning(Time_t time, TaskId_t task_id) {
    sla_warning_count++;
    pstate_pressure_cooldown = 10;
    SimOutput("SLAWarning(): Warning for task " + to_string(task_id) + " at " + to_string(time), 1);

    // Promote violating task to highest priority immediately.
    SetTaskPriority(task_id, HIGH_PRIORITY);

    // Wake one compatible sleeping machine to increase near-term capacity.
    CPUType_t required_cpu = RequiredCPUType(task_id);
    for (MachineId_t mid = 0; mid < Machine_GetTotal(); mid++) {
        MachineInfo_t m = Machine_GetInfo(mid);
        if (m.cpu != required_cpu) {
            continue;
        }
        if (m.s_state == S0) {
            continue;
        }
        Machine_SetState(mid, S0);
        break;
    }
}

void StateChangeComplete(Time_t time, MachineId_t machine_id) {
    // Called in response to an earlier request to change the state of a machine
    (void)machine_id;
    Scheduler.PeriodicCheck(time);
}

