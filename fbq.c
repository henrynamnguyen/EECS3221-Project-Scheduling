/*
Family Name: Nguyen
Given Name: Nhat Nam
Student Number: 218482182
EECS Login ID (the one you use to access the red server): nhatnam@red.yorku.ca
YorkU email address (the one that appears in eClass): nhatnam@my.yorku.ca
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "sch-helpers.h" /* include header file of all helper functions */
#define NUMBER_OF_LEVELS 3

/* Declare some global variables and structs to be used by FCFS scheduler */

process processes[MAX_PROCESSES+1]; /* a large array to hold all processes read
from data file */
/* these processes are pre-sorted and ordered
by arrival time */
int numberOfProcesses; /* total number of processes */
int nextProcess; /* index of next process to arrive */
process_queue readyQueue[NUMBER_OF_LEVELS]; /* ready queue to hold all ready processes */
process_queue waitingQueue; /* waiting queue to hold all processes in I/O waiting */
process *cpus[NUMBER_OF_PROCESSORS]; /* processes running on each cpu */
int totalWaitingTime; /* total time processes spent waiting */
int totalContextSwitches; /* total number of preemptions */
int simulationTime; /* time steps simulated */
int cpuTimeUtilized; /* time steps each cpu was executing */

int timeQuantums[NUMBER_OF_LEVELS-1];

/* holds processes moving to the ready queue this timestep (for sorting) */
process *preReadyQueue[MAX_PROCESSES+1];
int preReadyQueueSize;

int runningCPU;

/** Initialization functions **/

/* performs basic initialization on all global variables */
void initializeGlobals(void) {
    int i = 0;
    for (;i<NUMBER_OF_PROCESSORS;i++) {
        cpus[i] = NULL;
    }

    simulationTime = 0;
    cpuTimeUtilized = 0;
    totalWaitingTime = 0;
    totalContextSwitches = 0;
    numberOfProcesses = 0;
    nextProcess = 0;

    preReadyQueueSize = 0;
    
    // runningCPU = 0;

    for (i=0;i<NUMBER_OF_LEVELS;i++){
        initializeProcessQueue(&readyQueue[i]);
    }
    initializeProcessQueue(&waitingQueue);
}

/** FCFS scheduler simulation functions **/

/* compares processes pointed to by *aa and *bb by process id,
returning -1 if aa < bb, 1 if aa > bb and 0 otherwise. */
int compareProcessPointers(const void *aa, const void *bb) {
    process *a = *((process**) aa);
    process *b = *((process**) bb);
    if (a->pid < b->pid) {
        return -1;
    }
    if (a->pid > b->pid) {
        return 1;
    }
    assert(0); /* case should never happen, o/w ambiguity exists */
    return 0;
}

/* returns the number of processes currently executing on processors */
int runningProcesses(void) {
    int result = 0;
    int i;
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) {
        if (cpus[i] != NULL) {
            result++;
        }
    }
    return result;
}

/* returns the number of processes that have yet to arrive in the system */
int incomingProcesses(void) {
    return numberOfProcesses - nextProcess;
}

/* simulates the CPU scheduler, fetching and dequeuing the next scheduled
process from the ready queue. it then returns a pointer to this process,
or NULL if no suitable next process exists. */
process *nextScheduledProcess(void) {
    int readyQueueSize1 = readyQueue[0].size;
	int readyQueueSize2 = readyQueue[1].size;
	int readyQueueSize3 = readyQueue[2].size;
	process *front;

	if (readyQueueSize1 == 0 && readyQueueSize2 == 0 && readyQueueSize3 == 0){
        return NULL;
    }

	if (readyQueueSize1 != 0){
		front = readyQueue[0].front->data;
		dequeueProcess(&readyQueue[0]);
	}
	else if (readyQueueSize2 != 0){
		front = readyQueue[1].front->data;
		dequeueProcess(&readyQueue[1]);
	} 
	else if (readyQueueSize3 != 0){
		front = readyQueue[2].front->data;
		dequeueProcess(&readyQueue[2]);
	}
    return front;

}

/* enqueue newly arriving processes in the ready queue */
void moveIncomingProcesses(void) {
    /* place newly arriving processes into an intermediate array
    so that they will be sorted by priority and added to the ready queue */
    //Condition: as long as the next process does not exceed the total number of processes AND the arrivalTime of the process does not exceed total simulationTime
    while (nextProcess < numberOfProcesses && processes[nextProcess].arrivalTime <= simulationTime) {
        preReadyQueue[preReadyQueueSize] = &processes[nextProcess];
        preReadyQueue[preReadyQueueSize]->quantumRemaining = timeQuantums[0];
        preReadyQueueSize++;
        nextProcess++;
    }
}

/* move any waiting processes that are finished their I/O bursts to ready */
void moveWaitingProcesses(void) {
    // printf("moveWaiting\n");
    int i;
    int size = waitingQueue.size;

    /* place processes finished their I/O bursts into an intermediate array
    so that they will be sorted by priority and added to the ready queue */
    for (i=0;i<size;i++) {
        process *front = waitingQueue.front->data; /* get process at front */
        dequeueProcess(&waitingQueue); /* dequeue it */

        assert(front->bursts[front->currentBurst].step <=
        front->bursts[front->currentBurst].length);

        /* if process' current (I/O) burst is finished,
        move it to the ready queue, else return it to the waiting queue */
        if (front->bursts[front->currentBurst].step == front->bursts[front->currentBurst].length) {

            /* switch to next (CPU) burst and place in ready queue */
            front->currentBurst++;
            
            //Processes from waiting queue have quantumRemaining reset
            front->quantumRemaining = timeQuantums[0];
            front->currentQueue = 0;
 			front->endTime = simulationTime;

            preReadyQueue[preReadyQueueSize++] = front;
        } else {
            enqueueProcess(&waitingQueue, front);
        }
    }
}

/* move ready processes into free cpus according to scheduling algorithm */
void moveReadyProcesses(void) {
    int i;

    /* sort processes in the intermediate preReadyQueue array by pid,
    and add them to the ready queue prior to moving ready procs. into CPUs */
    qsort(preReadyQueue, preReadyQueueSize, sizeof(process*),compareProcessPointers);
    for (i=0;i<preReadyQueueSize;i++) {
        enqueueProcess(&readyQueue[0], preReadyQueue[i]);
        preReadyQueue[i] = NULL; //???
    }
    preReadyQueueSize = 0;
    
    /* for each idle cpu, load and begin executing
    the next scheduled process from the ready queue. */
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) {
        if (cpus[i] == NULL) {
            process *nextProcess = nextScheduledProcess();
            cpus[i] = nextProcess;
        }
    }
}

/* move any running processes that have finished their CPU burst to waiting,
and terminate those that have finished their last CPU burst. */
void moveRunningProcesses(void) {
    int i,j,k;
    process *preemptive1[NUMBER_OF_PROCESSORS];
    process *preemptive2[NUMBER_OF_PROCESSORS];
    int num1 = 0;
    int num2 = 0;
    
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) {
        //if more than one cpu core is working but not all cpu cores are working
        if (runningProcesses() < NUMBER_OF_PROCESSORS) {
            if (cpus[i] != NULL){
               if (cpus[i]->currentQueue == 0 || cpus[i]->currentQueue == 1){
               if (cpus[i]->bursts[cpus[i]->currentBurst].step == cpus[i]->bursts[cpus[i]->currentBurst].length) {
                    /* start process' next (I/O) burst */
                    cpus[i]->currentBurst++;

                    /* move process to waiting queue if it is not finished */
                    if (cpus[i]->currentBurst < cpus[i]->numberOfBursts) {
                        enqueueProcess(&waitingQueue, cpus[i]);

                    /* otherwise (if currentBurts has reached its input total numberOfBursts), terminate it (don't put it back in the queue) */
                    } else {
                        cpus[i]->endTime = simulationTime;
                    }

                    /* stop executing the process
                    -since this will remove the process from the cpu immediately,
                    but the process is supposed to stop running at the END of
                    the current time step, we need to add 1 to the runtime */
                    cpus[i] = NULL;
                }
                /*if the process CPU bursts is not finished BUT the timeQuantum has reached 0, move the unfinished process to the preemptive buffer array for READY queue and reset its timeQuantum to the old user-inputted timeQuantum*/
                else if (cpus[i]->quantumRemaining == 0){
                    if (cpus[i]->currentQueue == 0){
                        preemptive1[num1] = cpus[i];
                        preemptive1[num1]->quantumRemaining = timeQuantums[1];
                        preemptive1[num1]->currentQueue = 1;
                        num1++;
                        totalContextSwitches++;
                        cpus[i] = NULL;
                    }
                    else if (cpus[i]->currentQueue == 1){
                        preemptive2[num2] = cpus[i];
                        preemptive2[num2]->currentQueue = 2;
                        num2++;
                        totalContextSwitches++;
                        cpus[i] = NULL;
                    }
                }
               }
               else if (cpus[i]->currentQueue == 2){
                   if (cpus[i]->bursts[cpus[i]->currentBurst].step == cpus[i]->bursts[cpus[i]->currentBurst].length) {
                    /* start process' next (I/O) burst */
                    cpus[i]->currentBurst++;

                    /* move process to waiting queue if it is not finished */
                    if (cpus[i]->currentBurst < cpus[i]->numberOfBursts) {
                        enqueueProcess(&waitingQueue, cpus[i]);

                    /* otherwise (if currentBurts has reached its input total numberOfBursts), terminate it (don't put it back in the queue) */
                    } else {
                        cpus[i]->endTime = simulationTime;
                    }

                    /* stop executing the process
                    -since this will remove the process from the cpu immediately,
                    but the process is supposed to stop running at the END of
                    the current time step, we need to add 1 to the runtime */
                    cpus[i] = NULL;
                }
               }
            }
        }
        /*if the CPU bursts has not finished AND the remaining timeQuantum has not reached 0 BUT all cores were occupied AND there is a higher priority process than the one running */
        else if (runningProcesses()== NUMBER_OF_PROCESSORS){
            if (cpus[i] != NULL){
            if (readyQueue[0].size != 0){
                if (cpus[i]->currentQueue == 1){
                    if (readyQueue[0].size != 0){
                        preemptive1[num1] = cpus[i];
                        preemptive1[num1]->quantumRemaining = timeQuantums[1];
                        num1++;
                        totalContextSwitches++;
                        cpus[i] = NULL;
                    }
                }
                else if (cpus[i]->currentQueue == 2){
                    if (readyQueue[0].size != 0){
                        preemptive2[num2] = cpus[i];
                        num2++;
                        totalContextSwitches++;
                        cpus[i] = NULL;
                    }
                }
                else if (cpus[i]->currentQueue == 0){
                    /* if process' current (CPU) burst is finished BUT the timeQuantum has not reached 0, terminating the process(meanwhile, step will still increment and timeQuantum will decrement)*/ 
                    printf("When Equal 4 readyQ 0 not 0, pid %d Queue %d CurrentBurst %d Step %d Length %d\n",cpus[i]->pid, cpus[i]->currentQueue, cpus[i]->currentBurst, cpus[i]->bursts[cpus[i]->currentBurst].step,cpus[i]->bursts[cpus[i]->currentBurst].length);
                    if (cpus[i]->bursts[cpus[i]->currentBurst].step == cpus[i]->bursts[cpus[i]->currentBurst].length) {
                        printf("Before %d\n", runningProcesses());
                        /* start process' next (I/O) burst */
                        cpus[i]->currentBurst++;

                        /* move process to waiting queue if it is not finished */
                        if (cpus[i]->currentBurst < cpus[i]->numberOfBursts) {
                            enqueueProcess(&waitingQueue, cpus[i]);

                        /* otherwise (if currentBurts has reached its input total numberOfBursts), terminate it (don't put it back in the queue) */
                        } else {
                            cpus[i]->endTime = simulationTime;
                        }

                        /* stop executing the process
                        -since this will remove the process from the cpu immediately,
                        but the process is supposed to stop running at the END of
                        the current time step, we need to add 1 to the runtime */
                        cpus[i] = NULL;
                    }
                    /*if the process CPU bursts is not finished BUT the timeQuantum has reached 0, move the unfinished process to the preemptive buffer array for READY queue and reset its timeQuantum to the old user-inputted timeQuantum*/
                    else if (cpus[i]->quantumRemaining == 0){
                        if (cpus[i]->currentQueue == 0){
                            preemptive1[num1] = cpus[i];
                            preemptive1[num1]->quantumRemaining = timeQuantums[1];
                            preemptive1[num1]->currentQueue = 1;
                            num1++;
                            totalContextSwitches++;
                            cpus[i] = NULL;
                        }
                        else if (cpus[i]->currentQueue == 1){
                            printf("Before %d\n", runningProcesses());
                            preemptive2[num2] = cpus[i];
                            preemptive2[num2]->currentQueue = 2;
                            num2++;
                            totalContextSwitches++;
                            cpus[i] = NULL;
                        }
                    }
                }
            }
            else if (readyQueue[1].size != 0){
                if (cpus[i]->currentQueue == 1){
                    printf("When Equal 4 ReadyQ 1 not 0, pid %d Queue %d CurrentBurst %d Step %d Length %d\n",cpus[i]->pid, cpus[i]->currentQueue, cpus[i]->currentBurst, cpus[i]->bursts[cpus[i]->currentBurst].step,cpus[i]->bursts[cpus[i]->currentBurst].length);
                    /* if process' current (CPU) burst is finished BUT the timeQuantum has not reached 0, terminating the process(meanwhile, step will still increment and timeQuantum will decrement)*/ 
                    if (cpus[i]->bursts[cpus[i]->currentBurst].step == cpus[i]->bursts[cpus[i]->currentBurst].length) {
                        printf("Before %d\n", runningProcesses());
                        /* start process' next (I/O) burst */
                        cpus[i]->currentBurst++;

                        /* move process to waiting queue if it is not finished */
                        if (cpus[i]->currentBurst < cpus[i]->numberOfBursts) {
                            enqueueProcess(&waitingQueue, cpus[i]);
                        /* otherwise (if currentBurts has reached its input total numberOfBursts), terminate it (don't put it back in the queue) */
                        } else {
                            cpus[i]->endTime = simulationTime;
                        }

                        /* stop executing the process
                        -since this will remove the process from the cpu immediately,
                        but the process is supposed to stop running at the END of
                        the current time step, we need to add 1 to the runtime */
                        cpus[i] = NULL;

                    }
                    /*if the process CPU bursts is not finished BUT the timeQuantum has reached 0, move the unfinished process to the preemptive buffer array for READY queue and reset its timeQuantum to the old user-inputted timeQuantum*/
                    else if (cpus[i]->quantumRemaining == 0){
                        if (cpus[i]->currentQueue == 0){
                            preemptive1[num1] = cpus[i];
                            preemptive1[num1]->quantumRemaining = timeQuantums[1];
                            preemptive1[num1]->currentQueue = 1;
                            num1++;
                            totalContextSwitches++;
                            cpus[i] = NULL;
                        }
                        else if (cpus[i]->currentQueue == 1){
                            preemptive2[num2] = cpus[i];
                            preemptive2[num2]->currentQueue = 2;
                            num2++;
                            totalContextSwitches++;
                            cpus[i] = NULL;
                        }
                    }
                }
                else if (cpus[i]->currentQueue == 2){
                    if (readyQueue[1].size != 0){
                        preemptive2[num2] = cpus[i];
                        num2++;
                        totalContextSwitches++;
                        cpus[i] = NULL;
                    }
                }
                else if (cpus[i]->currentQueue == 0){
                    printf("When Equal 4 readyQ 1 not 0, pid %d Queue %d CurrentBurst %d Step %d Length %d\n",cpus[i]->pid, cpus[i]->currentQueue, cpus[i]->currentBurst, cpus[i]->bursts[cpus[i]->currentBurst].step,cpus[i]->bursts[cpus[i]->currentBurst].length);
                    /* if process' current (CPU) burst is finished BUT the timeQuantum has not reached 0, terminating the process(meanwhile, step will still increment and timeQuantum will decrement)*/ 
                    if (cpus[i]->bursts[cpus[i]->currentBurst].step == cpus[i]->bursts[cpus[i]->currentBurst].length) {
                        /* start process' next (I/O) burst */
                        cpus[i]->currentBurst++;

                        /* move process to waiting queue if it is not finished */
                        if (cpus[i]->currentBurst < cpus[i]->numberOfBursts) {
                            enqueueProcess(&waitingQueue, cpus[i]);

                        /* otherwise (if currentBurts has reached its input total numberOfBursts), terminate it (don't put it back in the queue) */
                        } else {
                            cpus[i]->endTime = simulationTime;
                        }

                        /* stop executing the process
                        -since this will remove the process from the cpu immediately,
                        but the process is supposed to stop running at the END of
                        the current time step, we need to add 1 to the runtime */
                        cpus[i] = NULL;
                    }
                    /*if the process CPU bursts is not finished BUT the timeQuantum has reached 0, move the unfinished process to the preemptive buffer array for READY queue and reset its timeQuantum to the old user-inputted timeQuantum*/
                    else if (cpus[i]->quantumRemaining == 0){
                        if (cpus[i]->currentQueue == 0){
                            preemptive1[num1] = cpus[i];
                            preemptive1[num1]->quantumRemaining = timeQuantums[1];
                            preemptive1[num1]->currentQueue = 1;
                            num1++;
                            totalContextSwitches++;
                            cpus[i] = NULL;
                        }
                        else if (cpus[i]->currentQueue == 1){
                            preemptive2[num2] = cpus[i];
                            preemptive2[num2]->currentQueue = 2;
                            num2++;
                            totalContextSwitches++;
                            cpus[i] = NULL;
                        }
                    }
                }
            }
            else if (readyQueue[2].size !=0){
                /* if process' current (CPU) burst is finished BUT the timeQuantum has not reached 0, terminating the process(meanwhile, step will still increment and timeQuantum will decrement)*/ 
                if (cpus[i]->currentQueue==0 || cpus[i]->currentQueue==1){
                    /* if process' current (CPU) burst is finished BUT the timeQuantum has not reached 0, terminating the process(meanwhile, step will still increment and timeQuantum will decrement)*/ 
                    printf("When Equal 4 ReadyQ 2 not 0, pid %d Queue %d CurrentBurst %d Step %d Length %d\n",cpus[i]->pid, cpus[i]->currentQueue, cpus[i]->currentBurst, cpus[i]->bursts[cpus[i]->currentBurst].step,cpus[i]->bursts[cpus[i]->currentBurst].length);
                    if (cpus[i]->bursts[cpus[i]->currentBurst].step == cpus[i]->bursts[cpus[i]->currentBurst].length) {
                        /* start process' next (I/O) burst */
                        cpus[i]->currentBurst++;

                        /* move process to waiting queue if it is not finished */
                        if (cpus[i]->currentBurst < cpus[i]->numberOfBursts) {
                            enqueueProcess(&waitingQueue, cpus[i]);

                        /* otherwise (if currentBurts has reached its input total numberOfBursts), terminate it (don't put it back in the queue) */
                        } else {
                            cpus[i]->endTime = simulationTime;
                        }

                        /* stop executing the process
                        -since this will remove the process from the cpu immediately,
                        but the process is supposed to stop running at the END of
                        the current time step, we need to add 1 to the runtime */
                        cpus[i] = NULL;
                    }
                    /*if the process CPU bursts is not finished BUT the timeQuantum has reached 0, move the unfinished process to the preemptive buffer array for READY queue and reset its timeQuantum to the old user-inputted timeQuantum*/
                    else if (cpus[i]->quantumRemaining == 0){
                        if (cpus[i]->currentQueue == 0){
                            preemptive1[num1] = cpus[i];
                            preemptive1[num1]->quantumRemaining = timeQuantums[1];
                            preemptive1[num1]->currentQueue = 1;
                            num1++;
                            totalContextSwitches++;
                            cpus[i] = NULL;
                        }
                        else if (cpus[i]->currentQueue == 1){
                            preemptive2[num2] = cpus[i];
                            preemptive2[num2]->currentQueue = 2;
                            num2++;
                            totalContextSwitches++;
                            cpus[i] = NULL;
                        }
                    }
                }
                else if (cpus[i]->currentQueue == 2){
                    printf("Equal 4, pid %d Queue %d CurrentBurst %d Step %d Length %d\n",cpus[i]->pid, cpus[i]->currentQueue, cpus[i]->currentBurst, cpus[i]->bursts[cpus[i]->currentBurst].step,cpus[i]->bursts[cpus[i]->currentBurst].length);
                    if (cpus[i]->bursts[cpus[i]->currentBurst].step == cpus[i]->bursts[cpus[i]->currentBurst].length) {
                        /* start process' next (I/O) burst */
                        cpus[i]->currentBurst++;

                        /* move process to waiting queue if it is not finished */
                        if (cpus[i]->currentBurst < cpus[i]->numberOfBursts) {
                            enqueueProcess(&waitingQueue, cpus[i]);

                        /* otherwise (if currentBurts has reached its input total numberOfBursts), terminate it (don't put it back in the queue) */
                        } else {
                            cpus[i]->endTime = simulationTime;
                        }

                        /* stop executing the process
                        -since this will remove the process from the cpu immediately,
                        but the process is supposed to stop running at the END of
                        the current time step, we need to add 1 to the runtime */
                        cpus[i] = NULL;
                    }
                }   
            }
            else {
                if (cpus[i] != NULL){
                    /* if process' current (CPU) burst is finished BUT the timeQuantum has not reached 0, terminating the process(meanwhile, step will still increment and timeQuantum will decrement)*/ 
                    printf("When Equal 4 all readyQ 0, pid %d Queue %d CurrentBurst %d Step %d Length %d\n",cpus[i]->pid, cpus[i]->currentQueue, cpus[i]->currentBurst, cpus[i]->bursts[cpus[i]->currentBurst].step,cpus[i]->bursts[cpus[i]->currentBurst].length);
                    if (cpus[i]->bursts[cpus[i]->currentBurst].step == cpus[i]->bursts[cpus[i]->currentBurst].length) {
                        /* start process' next (I/O) burst */
                        cpus[i]->currentBurst++;

                        /* move process to waiting queue if it is not finished */
                        if (cpus[i]->currentBurst < cpus[i]->numberOfBursts) {
                            enqueueProcess(&waitingQueue, cpus[i]);

                        /* otherwise (if currentBurts has reached its input total numberOfBursts), terminate it (don't put it back in the queue) */
                        } else {
                            cpus[i]->endTime = simulationTime;
                        }

                        /* stop executing the process
                        -since this will remove the process from the cpu immediately,
                        but the process is supposed to stop running at the END of
                        the current time step, we need to add 1 to the runtime */
                        cpus[i] = NULL;
                    }
                    /*if the process CPU bursts is not finished BUT the timeQuantum has reached 0, move the unfinished process to the preemptive buffer array for READY queue and reset its timeQuantum to the old user-inputted timeQuantum*/
                    else if (cpus[i]->quantumRemaining == 0){
                        if (cpus[i]->currentQueue == 0){
                            preemptive1[num1] = cpus[i];
                            preemptive1[num1]->quantumRemaining = timeQuantums[1];
                            preemptive1[num1]->currentQueue = 1;
                            num1++;
                            totalContextSwitches++;
                            cpus[i] = NULL;
                        }
                        else if (cpus[i]->currentQueue == 1){
                            preemptive2[num2] = cpus[i];
                            preemptive2[num2]->currentQueue = 2;
                            num2++;
                            totalContextSwitches++;
                            cpus[i] = NULL;
                        }
                    }
                }
            }
            }
        }
    }
    qsort(preemptive1, num1, sizeof(process*), compareProcessPointers);
    qsort(preemptive2, num2, sizeof(process*), compareProcessPointers);
    for (j=0; j < num1; j++){
        enqueueProcess(&readyQueue[1], preemptive1[j]);
    }
    for (k=0; k < num2; k++){
        enqueueProcess(&readyQueue[2], preemptive2[k]);
    }
}

/* increment each waiting process' current I/O burst's progress */
void updateWaitingProcesses(void) {
    int i;
    int size = waitingQueue.size;
    for (i=0;i<size;i++) {
        process *front = waitingQueue.front->data; /* get process at front */
        dequeueProcess(&waitingQueue); /* dequeue it */

        /* increment the current (I/O) burst's step (progress) */
        front->bursts[front->currentBurst].step++;
        enqueueProcess(&waitingQueue, front); /* enqueue it again */
    }
}

/* increment waiting time for each process in the ready queue */
void updateReadyProcesses(void) {
    int i,j;
    for (i=0;i<NUMBER_OF_LEVELS;i++){
        for (j=0;j<readyQueue[i].size;j++) {
            process *front = readyQueue[i].front->data; /* get process at front */
            dequeueProcess(&readyQueue[i]); /* dequeue it */
            front->waitingTime++; /* increment waiting time */
            enqueueProcess(&readyQueue[i], front); /* enqueue it again */
        }
    }
}

/* update the progress for all currently executing processes */
void updateRunningProcesses(void) {
    int i;
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) {
        if (cpus[i] != NULL) {
        /* increment the current (CPU) burst's step (progress) and decrement quantumRemaining for q0 and q1 processes */
            //printf("updateRunning\n");
            if (cpus[i]->currentQueue == 0 || cpus[i]->currentQueue == 1){
                cpus[i]->bursts[cpus[i]->currentBurst].step++;
                cpus[i]->quantumRemaining--;
            }
            else if (cpus[i]->currentQueue == 2 ){
                cpus[i]->bursts[cpus[i]->currentBurst].step++;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    int sumOfTurnaroundTimes = 0;
    int doneReading = 0;
    int i;
    timeQuantums[0] = atoi(argv[1]);
    timeQuantums[1] = atoi(argv[2]);
    
    
    /* read in all process data and populate processes array with the results */
    initializeGlobals();
    while (doneReading=readProcess(&processes[numberOfProcesses])) {
        if(doneReading==1) numberOfProcesses ++;
        if(numberOfProcesses > MAX_PROCESSES) break;
    }

    /* handle invalid number of processes in input */
    if (numberOfProcesses == 0) {
        fprintf(stderr, "Error: no processes specified in input.\n");
        return -1;
    } else if (numberOfProcesses > MAX_PROCESSES) {
        fprintf(stderr, "Error: too many processes specified in input; "
        "they cannot number more than %d.\n", MAX_PROCESSES);
        return -1;
    }

    /* sort the processes array ascending by arrival time */
    qsort(processes, numberOfProcesses, sizeof(process), compareByArrival);

    /* run the simulation */
    while (1) {
        moveIncomingProcesses(); /* admit any newly arriving processes */
        moveRunningProcesses(); /* move procs that shouldn't be running */
        moveWaitingProcesses(); /* move procs finished waiting to ready-Q */
        moveReadyProcesses(); /* move ready procs into any free cpu slots */

        updateWaitingProcesses(); /* update burst progress for waiting procs */
        updateReadyProcesses(); /* update waiting time for ready procs */
        updateRunningProcesses(); /* update burst progress for running procs */

        cpuTimeUtilized += runningProcesses();

        /* terminate simulation when:
        - no processes are running
        - no more processes await entry into the system
        - there are no waiting processes
        */
        //printf("Running %d\n",runningProcesses());
        if (runningProcesses() == 0 &&incomingProcesses() == 0 && waitingQueue.size == 0) break;
        
        simulationTime++;
    }

    /* compute and output performance metrics */
    for (i=0;i<numberOfProcesses;i++) {
        sumOfTurnaroundTimes += processes[i].endTime - processes[i].arrivalTime;
        totalWaitingTime += processes[i].waitingTime;
    }

    printf("Average waiting time : %.2f units\n"
    "Average turnaround time : %.2f units\n"
    "Time all processes finished : %d\n"
    "Average CPU utilization : %.1f%%\n"
    "Number of context switches : %d\n",
    totalWaitingTime / (double) numberOfProcesses,
    sumOfTurnaroundTimes / (double) numberOfProcesses,
    simulationTime,
    100.0 * cpuTimeUtilized / simulationTime,
    totalContextSwitches);

    printf("PID(s) of last process(es) to finish :");
    for (i=0;i<numberOfProcesses;i++) {
        if (processes[i].endTime == simulationTime) {
        printf(" %d", processes[i].pid);
        }
    }
    printf("\n");
    return 0;
}