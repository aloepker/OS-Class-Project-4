//Written by Adam Spencer Loepker
//Finished on October 14th, 2023
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <string.h>
#include <time.h>
#include <sys/msg.h>
#include <errno.h>
//Simulated clock functions and variables:
int sysClockNano = 0;
int sysClockSec = 0;
void incrementClock(){
	sysClockNano = sysClockNano + 100000000;
	if (sysClockNano > 1000000000){
		sysClockSec++;
		sysClockNano = sysClockNano - 1000000000;
	}
}
int randSeconds(int max){
	srand(time(NULL)+1);
	return rand()%max;
}
int randNano(){
	srand(time(NULL));
	return 1+rand()%1000000000;
}
//Process Control Block:
struct PCB {
	int occupied;
	pid_t pid;
	int startSeconds; //birth time seconds
	int startNano; //birth time nano
	int serviceTimeSec; //total seconds process has been running
	int serviceTimeNano; //total nanoseconds process ha been running
	int eventWaitSec; //seconds before unblock
	int eventWaitNano; //nano before unblock
	int blocked; //is the process blocked?
};
struct PCB processTable[20];
//Display process control block:
void printPCB(int smS, int smN, FILE *file){
	printf("OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(), smS, smN);
	fprintf(file, "OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(), smS, smN);
	printf("Process Table:\n");
	fprintf(file, "Process Table:\n");
	printf("Entry Occupied PID    StartS StartN\n");
	fprintf(file, "Entry Occupied PID    StartS StartN\n");
	int m;
	for (m=0;m<20;m++){
		printf("  %d     %d      %d     %d      %d\n", m, processTable[m].occupied, processTable[m].pid, processTable[m].startSeconds, processTable[m].startNano);
		fprintf(file, "  %d     %d      %d     %d      %d\n", m, processTable[m].occupied, processTable[m].pid, processTable[m].startSeconds, processTable[m].startNano);
	}
}
//help function:
void help(){
	printf("The options for the program are:\n");
	printf("-n <number>   this sets the number of processes to launch\n");
	printf("-s <number>   this sets how many processes can run at once\n");
	printf("-t <number>   this sets the maximum time in seconds a random number generator can chose from for the lifespan of the worker processes\n");
	printf("-f <\"output_file_name.txt\">   this sets the name of the output file\n");
	printf("example:\n./oss -n 3 -s 2 -t 3 -f \"output.txt\"\n");
}
//Shared memory and message queue constants:
#define SHMKEY 859048
#define BUFF_SZ sizeof (int)

#define PERMS 0777
typedef struct msgbuffer {
	long mtype;
	char strData[100];
	int intData;
} msgbuffer;

// oss goals:
//initianlze components
	//while(termination flags are not set) loop:
	//check for active workers in pcb: if none, increment time by -t, else increment by less. set 1 of 2 termination flags
	//checked blocked processes to see if unblocked time has passed and act accordingly
	//check time ratio of unblocked processes to schedule, if any: if so, send then blocking wait for message. handle other message return processes too.
	//increment time (tentitive location in the loop)
	//use logic to see if a new process could/should be forked.if so, set new nano to 1, if total has  launched, set a kill flag.(reset a flag at launch maybe.
//end loop
//print system report

//order of progress:
//1. start with messaging 1 worker back and forth. get worker to run its loop corrctly.
//2. update to total number of workers desired, add in check to see if a process should be launched.
//3. screen and file outputs, system report included.
// ask about signal handling for remaining processes and 3 seconds for oss to stop launching processes

int main(int argc, char** argv){
	int option;
	int numWorkers = 0;
	int workerLimit = 0;
	int timeLimit = 0;
	int prevSec = 0;
	char *logFile= "log_file.txt";
	//User argument menu:
	while((option = getopt(argc, argv, "hn:s:t:f:")) != -1){
		switch(option){
			case 'h':
				help();
				return EXIT_SUCCESS;
			case 'n':
				numWorkers = atoi(optarg);
				break;
			case 's':
				workerLimit = atoi(optarg);
				break;
			case 't':
				timeLimit = atoi(optarg);
				break;
			case 'f':
				logFile = optarg;
				break;
			case '?':
				if ((optopt = 'c')){
					printf("Option %c requires an argument\n", optopt);
				} else if (isprint(optopt)){
					printf("Unknown Character '\\x%x'.\n", optopt);
				}
				return 1;
			default:
				help();
				return EXIT_SUCCESS;
		}
	}
	//open output file
	FILE *outputFile = fopen(logFile, "w");
	if (outputFile==NULL){
		printf("Error opening output file!\nTerminating program\n");
		return EXIT_FAILURE;
	}
	//initializing shared memory
	int shmid = shmget ( SHMKEY, BUFF_SZ, 0777 | IPC_CREAT );
	if ( shmid == -1 ){
		perror("Shared Memory Creation Error!!!\n");
		return EXIT_FAILURE;
	}
	char * paddr = ( char * )( shmat ( shmid, 0, 0 ) );
	int * shmTime = ( int * )( paddr );
	shmTime[0]=0;
	shmTime[1]=0;
	//message que initial implementation:
	msgbuffer buf1;
	int msqid;
	key_t key;
	system("touch msgq.txt");
	//set message queue key
	if ((key = ftok("msgq.txt", 1)) == -1) {
		perror("parent ftok error");
		exit(1);
	}
	//creates message queue
	if ((msqid = msgget(key, PERMS | IPC_CREAT)) == -1) {
		perror("msgget in parent error");
		exit(1);
	}
	//print user input verification:
	printf("OSS: Number of workers Selected: %d\nNumber of Workers at a time: %d\nNumber of loops for each Worker: %d\nOutput file: %s\n", numWorkers, workerLimit, timeLimit,logFile);
	fprintf(outputFile, "OSS: Number of workers Selected: %d\nNumber of Workers at a time: %d\nNumber of loops for each Worker: %d\nOutput file: %s\n", numWorkers, workerLimit, timeLimit,logFile);
	int i=0,j=0,n=0;
	//fork calls:
	pid_t childPid;
	msgbuffer rcvbuf;
	int createdWorkers = 0;
	int activeWorkers = 0;
	int isWorkerActive = 0;
	int nanoFlag = 0;
	//while workers still active:
	while (i<numWorkers){
		//increment the clock
		incrementClock();
		//update clock into shared memory
		shmTime[0] = sysClockSec;
		shmTime[1] = sysClockNano;
		if(shmTime[1] > 500000000 && nanoFlag == 0){
			nanoFlag = 1;
		printPCB(shmTime[0], shmTime[1], outputFile);
		}else if(shmTime[0] > prevSec){
			nanoFlag = 0;
			printPCB(shmTime[0], shmTime[1], outputFile);
		}
		//if can create worker, create worker and update PCB:
		if (createdWorkers < numWorkers){
			if (activeWorkers < workerLimit ){
				childPid = fork();
				if (childPid == -1){
					printf("Fork Process Failed!\n");
					return EXIT_FAILURE;
				}
				//child side of the fork if
				if (childPid == 0) {
					int timeSec = randSeconds(timeLimit);
					int timeNano = randNano();
					char secArg[10];
					char nanoArg[10];
					sprintf(secArg, "%d", timeSec);
					sprintf(nanoArg, "%d", timeNano);
					char * args[] = {"./worker", secArg, nanoArg, NULL};
					execvp("./worker", args);
				}
				//parent side of fork if
				createdWorkers++;
				activeWorkers++;
				//update pcb entry after a fork:
				processTable[n].occupied = 1;
				processTable[n].pid = childPid;
				processTable[n].startSeconds = sysClockSec;
				processTable[n].startNano = sysClockNano;
				n++;
			}
		}
		//message workers in sequence:
		//find next occupied pcb entry:
		while(isWorkerActive != 1){
			if(processTable[j].occupied == 1){
				isWorkerActive = 1;
			}else{
				j++;
				if (j==20){
					j=0;
				}
			}
		}
			//send and recieve a message with the selected pcb entry:
			buf1.mtype = processTable[j].pid;
			buf1.intData = processTable[j].pid;
			printf("OSS: Sending message to worker %d PID %d at time %d:%d\n", j, processTable[j].pid, sysClockSec, sysClockNano);
			fprintf(outputFile, "OSS: Sending message to worker %d PID %d at time %d:%d\n", j, processTable[j].pid, sysClockSec, sysClockNano);
			strcpy(buf1.strData, "Message to Child form Parent");
			if ((msgsnd(msqid, &buf1, sizeof(msgbuffer)-sizeof(long), 0)) == -1) {
				perror("msgsnd to child failed");
				exit(1);
			}
			printf("OSS: Recieving message from worker %d PID %d at time %d:%d\n", j, processTable[j].pid, sysClockSec, sysClockNano);
			fprintf(outputFile, "OSS: Recieving message from worker %d PID %d at time %d:%d\n", j, processTable[j].pid, sysClockSec, sysClockNano);
			if (msgrcv(msqid, &rcvbuf, sizeof(msgbuffer), getpid(), 0) == -1){
				perror("failed to recieve message in parent\n");
				exit(1);
			}
			//if worker terminates, update pcb:
			if (rcvbuf.intData == 0){
			printf("OSS: Worker %d PID %d is planing ot terminate.\n", j, processTable[j].pid);
			fprintf(outputFile, "OSS: Worker %d PID %d is planing ot terminate.\n", j, processTable[j].pid);
				activeWorkers--;
				i++;
				processTable[j].occupied = 0;
			}
		//limits pcb searching loop
		isWorkerActive = 0;
		if (activeWorkers == 0){
			isWorkerActive = 1;
		}
		j++;
	}
		//close shared memory and output file:
		shmdt(shmTime);
		shmctl(shmid, IPC_RMID, NULL);
		fclose(outputFile);
		//clear message ques:
		if (msgctl(msqid, IPC_RMID, NULL) == -1){
			perror("msgctl failed to get rid of que in parent ");
			exit(1);
		}
}
