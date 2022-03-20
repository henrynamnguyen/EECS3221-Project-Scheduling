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

/* Declare some global variables and structs to be used by FCFS scheduler */

process processes[MAX_PROCESSES+1]; /* a large array to hold all processes read
from data file */
/* these processes are pre-sorted and ordered
by arrival time */
int numberOfProcesses; /* total number of processes */
int nextProcess; /* index of next process to arrive */
process_queue readyQueue; /* ready queue to hold all ready processes */
process_queue waitingQueue; /* waiting queue to hold all processes in I/O waiting */
process *cpus[NUMBER_OF_PROCESSORS]; /* processes running on each cpu */
int totalWaitingTime; /* total time processes spent waiting */
int totalContextSwitches; /* total number of preemptions */
int simulationTime; /* time steps simulated */
int cpuTimeUtilized; /* time steps each cpu was executing */

int timeQuantum;

/* holds processes moving to the ready queue this timestep (for sorting) */
process *preReadyQueue[MAX_PROCESSES+1];
int preReadyQueueSize;

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

    initializeProcessQueue(&readyQueue);
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
        if (cpus[i] != NULL) result++;
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
    if (readyQueue.size == 0) return NULL;
    process *result = readyQueue.front->data;
    dequeueProcess(&readyQueue);
    return result;
}

/* enqueue newly arriving processes in the ready queue */
void moveIncomingProcesses(void) {
    /* place newly arriving processes into an intermediate array
    so that they will be sorted by priority and added to the ready queue */
    //Condition: as long as the next process does not exceed the total number of processes AND the arrivalTime of the process does not exceed total simulationTime
    while (nextProcess < numberOfProcesses && processes[nextProcess].arrivalTime <= simulationTime) {
        preReadyQueue[preReadyQueueSize] = &processes[nextProcess];
        preReadyQueue[preReadyQueueSize]->quantumRemaining = timeQuantum;
        preReadyQueueSize++;
        //printf("%d ",preReadyQueueSize);
        nextProcess++;
    }
}

/* move any waiting processes that are finished their I/O bursts to ready */
void moveWaitingProcesses(void) {
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
            
            //NEW
            front->quantumRemaining = timeQuantum;
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
    qsort(preReadyQueue, preReadyQueueSize, sizeof(process*),
    compareProcessPointers);
    for (i=0;i<preReadyQueueSize;i++) {
        enqueueProcess(&readyQueue, preReadyQueue[i]);
        preReadyQueue[i] = NULL; //???
    }
    preReadyQueueSize = 0;

    /* for each idle cpu, load and begin executing
    the next scheduled process from the ready queue. */
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) {
        if (cpus[i] == NULL) {
            cpus[i] = nextScheduledProcess();
        }
    }
}

/* move any running processes that have finished their CPU burst to waiting,
and terminate those that have finished their last CPU burst. */
void moveRunningProcesses(void) {
    int i,j;
    process *preemptive[NUMBER_OF_PROCESSORS];
    int num = 0;
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) {
        //if the cpu core is working
        if (cpus[i] != NULL) {
        /* if process' current (CPU) burst is finished , meanwhile if its not finished, step will still increment and timeQuantum will decrement*/ 
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
            /*if the process CPU bursts is not finished but the timeQuantum has reached 0, move the unfinished process to the preemptive buffer array for READY queue and reset its timeQuantum to the old user-inputted timeQuantum*/
            else if(cpus[i]->quantumRemaining == 0){
 				preemptive[num] = cpus[i];
 				preemptive[num]->quantumRemaining = timeQuantum;
 				num++;
 				totalContextSwitches++;
 				cpus[i] = NULL;
 			}
        }
    }
    // sort preemptive processes by process ID's and enqueue in the ready queue
 	qsort(preemptive, num, sizeof(process*), compareProcessPointers);
 	for (j = 0; j < num; j++){
 		enqueueProcess(&readyQueue, preemptive[j]);
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
    int i;
    for (i=0;i<readyQueue.size;i++) {
        process *front = readyQueue.front->data; /* get process at front */
        dequeueProcess(&readyQueue); /* dequeue it */
        front->waitingTime++; /* increment waiting time */
        enqueueProcess(&readyQueue, front); /* enqueue it again */
    }
}

/* update the progress for all currently executing processes */
void updateRunningProcesses(void) {
    int i;
    for (i=0;i<NUMBER_OF_PROCESSORS;i++) {
        if (cpus[i] != NULL) {
        /* increment the current (CPU) burst's step (progress) */
            cpus[i]->bursts[cpus[i]->currentBurst].step++;
            cpus[i]->quantumRemaining--;
        }
    }
}

int main(int argc, char *argv[]) {
    int sumOfTurnaroundTimes = 0;
    int doneReading = 0;
    int i;
    timeQuantum = atoi(argv[1]);
    
    
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
        printf("Running processes:%d Unarriving processes:%d WaitingQ processes%d\n",runningProcesses(),incomingProcesses(),waitingQueue.size);
        if (runningProcesses() == 0 &&
        incomingProcesses() == 0 &&
        waitingQueue.size == 0) break;

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