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
int currentLevel;

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
    
    currentLevel = 0;
    preReadyQueueSize = 0;

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

/**
 * Compare priorities of two processes
 */
int comparePriority(const void *a, const void *b){
	process *first = (process *) a;
	process *second = (process *) b;
	if (first->priority == second->priority){
		return compareProcessPointers(first, second);
	}
	return first->priority - second->priority;
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
    int readyQueueSize1 = readyQueue[0].size;
	int readyQueueSize2 = readyQueue[1].size;
	int readyQueueSize3 = readyQueue[2].size;
	process *grabNext;
	
	if (readyQueueSize1 != 0){
		grabNext = readyQueue[0].front->data;
		dequeueProcess(&readyQueue[0]);
	}
	else if (readyQueueSize2 != 0){
		grabNext = readyQueue[1].front->data;
		dequeueProcess(&readyQueue[1]);
	} 
	else if (readyQueueSize3 != 0){
		grabNext = readyQueue[2].front->data;
		dequeueProcess(&readyQueue[2]);
	}
	return grabNext; 
}

/* enqueue newly arriving processes in the ready queue */
void moveIncomingProcesses(void) {
    /* place newly arriving processes into an intermediate array
    so that they will be sorted by priority and added to the ready queue */
    printf("moveIncoming\n");
    while (nextProcess < numberOfProcesses && processes[nextProcess].arrivalTime <= simulationTime) {
        preReadyQueue[preReadyQueueSize] = &processes[nextProcess];
        //preReadyQueue[preReadyQueueSize]->priority = 0;
        preReadyQueueSize++;
        nextProcess++;
    }
}

/* move any waiting processes that are finished their I/O bursts to ready */
void moveWaitingProcesses(void) {
    printf("moveWaiting\n");
    int i;
    int size = waitingQueue.size;
 
    /* place processes finished their I/O bursts into an intermediate array
    so that they will be sorted by priority and added to the ready queue */
    for (i=0;i<size;i++) {
        process *front = waitingQueue.front->data; /* get process at front */
        front->priority = 0;
 		front->quantumRemaining = 0;
        dequeueProcess(&waitingQueue); /* dequeue it */

        assert(front->bursts[front->currentBurst].step <=
        front->bursts[front->currentBurst].length);

        /* if process' current (I/O) burst is finished,
        move it to the ready queue, else return it to the waiting queue */
        if (front->bursts[front->currentBurst].step == front->bursts[front->currentBurst].length) {

            /* switch to next (CPU) burst and place in ready queue */
            front->currentBurst++;
            
            //NEW
 			front->endTime = simulationTime;

            preReadyQueue[preReadyQueueSize++] = front;
        } else {
            enqueueProcess(&waitingQueue, front);
        }
    }
}

/**
 * Sort elements in "pre-ready queue" in order to add them to the ready queue
 * in the proper order. Enqueue all processes in "pre-ready queue" to ready queue.
 * Reset "pre-ready queue" size to 0. Find a CPU that doesn't have a process currently 
 * running on it and schedule the next process on that CPU
 */
void moveReadyProcesses(void) {
    int i;
    printf("moveReady\n");
    /* sort processes in the intermediate preReadyQueue array by pid,
    and add them to the ready queue prior to moving ready procs. into cpus */
    
            qsort(preReadyQueue, preReadyQueueSize, sizeof(process*),compareProcessPointers);
            for (i=0;i<preReadyQueueSize;i++) {
                enqueueProcess(&readyQueue[0], preReadyQueue[i]);
                preReadyQueue[i]=NULL;
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

/**
 * Check all cases; first that the first time slice hasn't expired, if it has then move
 * process to the second level. Run the process on that level, if the time slice expires
 * then move it to the fcfs part of the algorithm. There it will operate as a regular fcfs
 * algorithm, processing each process in a first come first serve manner.
 */
void moveRunningProcesses(void) {
    printf("moveRunning\n");
    int readyQueueSize1 = readyQueue[0].size;
	int readyQueueSize2 = readyQueue[1].size;
	int readyQueueSize3 = readyQueue[2].size;
 	int i;
 	
 	for (i = 0; i < NUMBER_OF_PROCESSORS; i++){
 		if (cpus[i] != NULL){
 			if (cpus[i]->bursts[cpus[i]->currentBurst].step != cpus[i]->bursts[cpus[i]->currentBurst].length 
 				&& cpus[i]->quantumRemaining != timeQuantums[0] && cpus[i]->priority == 0){
 				cpus[i]->quantumRemaining++;
 				cpus[i]->bursts[cpus[i]->currentBurst].step++;
                 printf("CPU not Leaving1\n");
 			}
 			else if(cpus[i]->bursts[cpus[i]->currentBurst].step != cpus[i]->bursts[cpus[i]->currentBurst].length 
 				&& cpus[i]->quantumRemaining == timeQuantums[0] && cpus[i]->priority == 0){
 				cpus[i]->quantumRemaining = 0;
 				cpus[i]->priority = 1;
 				totalContextSwitches++;
 				enqueueProcess(&readyQueue[1], cpus[i]);
 				cpus[i] = NULL;
                 printf("CPU Leaving1\n");
 			}
 			else if(cpus[i]->bursts[cpus[i]->currentBurst].step != cpus[i]->bursts[cpus[i]->currentBurst].length 
 				&& cpus[i]->quantumRemaining != timeQuantums[1] && cpus[i]->priority == 1){
 				if (readyQueueSize1 != 0){
 					cpus[i]->quantumRemaining = 0;
 					totalContextSwitches++;
 					enqueueProcess(&readyQueue[1], cpus[i]);
 					cpus[i] = NULL;
                     printf("CPU Leaving2\n");
 				}
 				else{
 					cpus[i]->bursts[cpus[i]->currentBurst].step++;
 					cpus[i]->quantumRemaining++;
                      printf("CPU not Leaving2\n");
 				}
 			}
 			else if(cpus[i]->bursts[cpus[i]->currentBurst].step != cpus[i]->bursts[cpus[i]->currentBurst].length 
 				&& cpus[i]->quantumRemaining == timeQuantums[1] && cpus[i]->priority == 1){
 				cpus[i]->quantumRemaining = 0;
 				cpus[i]->priority = 2;
 				totalContextSwitches++;
 				enqueueProcess(&readyQueue[2], cpus[i]);
 				cpus[i] = NULL;
                 printf("CPU Leaving3\n");
 			}
 			else if(cpus[i]->bursts[cpus[i]->currentBurst].step != cpus[i]->bursts[cpus[i]->currentBurst].length 
 				&& cpus[i]->priority == 2){
 				if (readyQueueSize1 != 0 || readyQueueSize2 != 0){
 					cpus[i]->quantumRemaining = 0;
 					totalContextSwitches++;
 					enqueueProcess(&readyQueue[2], cpus[i]);
 					cpus[i] = NULL;
                    printf("CPU Leaving4\n");
 				}
 				else{
 					cpus[i]->bursts[cpus[i]->currentBurst].step++;
                      printf("CPU not Leaving3\n");
 				}
 			}
 			// fcfs part of the algorithm
 			else if (cpus[i]->bursts[cpus[i]->currentBurst].step == cpus[i]->bursts[cpus[i]->currentBurst].length){
 				cpus[i]->currentBurst++;
 				cpus[i]->quantumRemaining = 0;
 				cpus[i]->priority = 0;
 				if(cpus[i]->currentBurst < cpus[i]->numberOfBursts){
 					enqueueProcess(&waitingQueue, cpus[i]);
 				}
 				else{
 					cpus[i]->endTime = simulationTime;
 				}
 				cpus[i] = NULL;
                printf("CPU Leaving5\n");
 			}
 		}
 		// if the CPU is free, assign it work
 		else if (cpus[i] == NULL){
 			if(readyQueueSize1 != 0){
 				process *front = readyQueue[0].front->data;
 				dequeueProcess(&readyQueue[0]);
 				cpus[i] = front;
 				cpus[i]->bursts[cpus[i]->currentBurst].step++;
 				cpus[i]->quantumRemaining++;
			}
			else if(readyQueueSize2 != 0){
 				process *front = readyQueue[1].front->data;
 				dequeueProcess(&readyQueue[1]);
 				cpus[i] = front;
 				cpus[i]->bursts[cpus[i]->currentBurst].step++;
 				cpus[i]->quantumRemaining++;
 			}
 			else if(readyQueueSize3 != 0){
 				process *front = readyQueue[2].front->data;
 				dequeueProcess(&readyQueue[2]);
 				cpus[i] = front;
 				cpus[i]->bursts[cpus[i]->currentBurst].step++;
 				cpus[i]->quantumRemaining = 0;
 			}
 		}	
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
        for (j=0;i<readyQueue[i].size;i++) {
            process *front = readyQueue[i].front->data; /* get process at front */
            dequeueProcess(&readyQueue[i]); /* dequeue it */
            front->waitingTime++; /* increment waiting time */
            enqueueProcess(&readyQueue[i], front); /* enqueue it again */
        }
    }
}

/* update the progress for all currently executing processes */
// void updateRunningProcesses(void) {
//     int i;
//     for (i=0;i<NUMBER_OF_PROCESSORS;i++) {
//         if (cpus[i] != NULL) {
//         /* increment the current (CPU) burst's step (progress) */
//             cpus[i]->bursts[cpus[i]->currentBurst].step++;
//             cpus[i]->quantumRemaining--;
//         }
//     }
// }

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
        // moveRunningProcesses(); /* move procs that shouldn't be running */
        // moveWaitingProcesses(); /* move procs finished waiting to ready-Q */
        // moveReadyProcesses(); /* move ready procs into any free cpu slots */
        moveReadyProcesses();
        moveRunningProcesses();
        moveWaitingProcesses();

        updateWaitingProcesses(); /* update burst progress for waiting procs */
        updateReadyProcesses(); /* update waiting time for ready procs */
        //updateRunningProcesses(); /* update burst progress for running procs */

        cpuTimeUtilized += runningProcesses();

        /* terminate simulation when:
        - no processes are running
        - no more processes await entry into the system
        - there are no waiting processes
        */
        printf("%d %d %d\n",runningProcesses(),incomingProcesses(),waitingQueue.size);
        if (runningProcesses() == 0 && incomingProcesses() == 0 && waitingQueue.size == 0) {
            break;
        }
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