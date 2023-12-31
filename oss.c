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
#include <float.h>
//Simulated clock functions and variables:
const int schTime = 50000;
int sysClockNano = 0;
int sysClockSec = 0;
void incrementClock(){
	sysClockNano = sysClockNano + 10000;
	if (sysClockNano > 1000000000){
		sysClockSec++;
		sysClockNano -= 1000000000;
	}
}
void incrementByX(int x){
	sysClockNano = sysClockNano + x;
	if (sysClockNano > 1000000000){
		sysClockSec++;
		sysClockNano -= 1000000000;
	}
}
int randSeconds(int max){
	return rand()%max;
}
int randNano(){
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
//void printPCB(int smS, int smN, FILE *file){
//	printf("OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(), smS, smN);
//	fprintf(file, "OSS PID: %d SysClockS: %d SysClockNano: %d\n", getpid(), smS, smN);
//	printf("Process Table:\n");
//	fprintf(file, "Process Table:\n");
//	printf("Entry Occupied PID    StartS StartN\n");
//	fprintf(file, "Entry Occupied PID    StartS StartN\n");
//	int m;
//	for (m=0;m<20;m++){
//		printf("  %d     %d      %d     %d      %d\n", m, processTable[m].occupied, processTable[m].pid, processTable[m].startSeconds, processTable[m].startNano);
//		fprintf(file, "  %d     %d      %d     %d      %d\n", m, processTable[m].occupied, processTable[m].pid, processTable[m].startSeconds, processTable[m].startNano);
//	}
//}
//help function:
void help(){
	printf("The options for the program are:\n");
	printf("-n <number>   this sets the number of processes to launch\n");
	printf("-s <number>   this sets how many processes can run at once\n");
	printf("-t <number>   this sets the maximum time in seconds a random number generator can chose from for the lifespan of the worker processes\n");
	printf("-f <\"output_file_name.txt\">   this sets the name of the output file\n");
	printf("example:\n./oss -n 3 -s 2 -t 3 -f \"output.txt\"\n");
}
//Message queue constants:
#define PERMS 0777
typedef struct msgbuffer {
	long mtype;
//	char strData[100];
	int intData;
} msgbuffer;

// oss goals:


//order of progress:
//1. start with messaging 1 worker back and forth. get worker to run its loop corrctly.
//2. update to total number of workers desired, add in check to see if a process should be launched.
//3. screen and file outputs, system report included.
// ask about signal handling for remaining processes and 3 seconds for oss to stop launching processes

int main(int argc, char** argv){
//initianlze components
	srand(time(NULL));
	int option;
	int numWorkers = 0;
	int workerLimit = 0;
	int time = 0;
//	int prevSec = 0;
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
				time = atoi(optarg);
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
printf("Message que active in parrent\n");
	//print user input verification:
	printf("OSS: Number of workers Selected: %d\nNumber of Workers at a time: %d\nNumber of loops for each Worker: %d\nOutput file: %s\n", numWorkers, workerLimit, time,logFile);
	fprintf(outputFile, "OSS: Number of workers Selected: %d\nNumber of Workers at a time: %d\nNumber of loops for each Worker: %d\nOutput file: %s\n", numWorkers, workerLimit, time,logFile);
	int i=0,j=0,n=0;
	//fork calls:
	pid_t childPid;
	msgbuffer rcvbuf;
//	int createdWorkers = 0;
	int totalNewWorkers = 0;
	int activeWorkers = 0;
//	int isWorkerActive = 0;
//	int nanoFlag = 0;
	int termFlag1 = 0;
	int termFlag2 = 0;
//	int lowestTimeS = 0;
//	int lowestTimeN = 0;
	int planToSchedule = 20;
	int totalSecActive;
	int totalNanoActive;
	int randomSecond;
	double secRatio;
	double nanoRatio;
	double lowSecRatio = 1;
	double lowNanoRatio = 1;

	//while(termination flags are not set) loop:
	while ((termFlag1 != 1) && (termFlag2 != 1)){
	//check for active workers in pcb: if none, increment time by -t, else increment by less. set 1 of 2 termination flags
	//also, checked blocked processes to see if unblocked time has passed and act accordingly
		termFlag1 = 0;
		activeWorkers = 0;
		planToSchedule = 20;
		lowSecRatio = 1;
		lowNanoRatio = 1;
		for(i=0;i<20;i++){
			//if occupied and blocked, check time for unblock, if time then unblock
			if(processTable[i].occupied == 1){
				activeWorkers++;
				if(processTable[i].blocked == 1){
					//check to see if time for unblocking has passed:
					if((sysClockSec > processTable[i].eventWaitSec) && (sysClockNano > processTable[i].eventWaitNano)){
						processTable[i].blocked = 0;
						incrementByX(5000);
					}
				}
//might want to move this part of the loop lower closer to scheduling
				if(processTable[i].blocked == 0){
					//check time ratio to see if it beats the lowest, if so, it becomes the next scheduled
					totalSecActive = sysClockSec - processTable[i].startSeconds;
					totalNanoActive = sysClockNano - processTable[i].startNano;
					secRatio = processTable[i].serviceTimeSec / totalSecActive;
					nanoRatio = processTable[i].serviceTimeNano / totalNanoActive;
					if((secRatio < lowSecRatio) && (nanoRatio < lowNanoRatio)){
						planToSchedule = i;
						lowSecRatio = secRatio;
						lowNanoRatio = nanoRatio;
					}
				}
			}
		}

		if ((activeWorkers == 0)){
			//increment by -t time initially and to allow a worker to fork. set flag incase worker max has been hit
			termFlag1 = 1;
			incrementByX(time);
		}else{
			incrementByX(5000);
		}

		//message workers by least ammount of runtime
		if ((planToSchedule != 20)) {
			//send and recieve a message with the selected pcb entry:
			buf1.mtype = processTable[planToSchedule].pid;
			buf1.intData = schTime;
			printf("OSS: Sending message to worker %d PID %d at time %d:%d\n", j, processTable[planToSchedule].pid, sysClockSec, sysClockNano);
			fprintf(outputFile, "OSS: Sending message to worker %d PID %d at time %d:%d\n", j, processTable[planToSchedule].pid, sysClockSec, sysClockNano);
			if ((msgsnd(msqid, &buf1, sizeof(msgbuffer)-sizeof(long), 0)) == -1) {
				perror("msgsnd to child failed");
				exit(1);
			}
			printf("OSS: Recieving message from worker %d PID %d at time %d:%d\n", j, processTable[planToSchedule].pid, sysClockSec, sysClockNano);
			fprintf(outputFile, "OSS: Recieving message from worker %d PID %d at time %d:%d\n", j, processTable[planToSchedule].pid, sysClockSec, sysClockNano);
			if (msgrcv(msqid, &rcvbuf, sizeof(msgbuffer), getpid(), 0) == -1){
				perror("failed to recieve message in parent\n");
				exit(1);
			}
//action based on workers response message: including pcb uptates!!

			if (rcvbuf.intData < 0){
				printf("OSS: Worker %d PID %d is planing ot terminate.\n", planToSchedule, processTable[planToSchedule].pid);
				fprintf(outputFile, "OSS: Worker %d PID %d is planing ot terminate.\n", planToSchedule, processTable[planToSchedule].pid);
//consider output for these statements
				incrementByX((rcvbuf.intData * (-1)));
				processTable[planToSchedule].serviceTimeNano += (rcvbuf.intData * -1);//redundant, consider a function instead..
				if ((processTable[planToSchedule].serviceTimeNano >= 1000000000)){
					processTable[planToSchedule].serviceTimeSec++;
					processTable[planToSchedule].serviceTimeNano -= 1000000000;
				}
				activeWorkers--;
				processTable[planToSchedule].occupied = 0;
			}else if(rcvbuf.intData < schTime){
				printf("OSS: Worker %d PID %d is requesting an IO opperation.\n", planToSchedule, processTable[planToSchedule].pid);
//must respond with time used
				incrementByX(rcvbuf.intData);
				processTable[planToSchedule].serviceTimeNano += rcvbuf.intData;
				if ((processTable[planToSchedule].serviceTimeNano >= 1000000000)){
					processTable[planToSchedule].serviceTimeSec++;
					processTable[planToSchedule].serviceTimeNano -= 1000000000;

				}
				processTable[planToSchedule].blocked = 1;
				//determine how long the IO opperation will make the program wait
				randomSecond = randNano();
				processTable[planToSchedule].eventWaitSec = (sysClockSec + randomSecond);
				processTable[planToSchedule].eventWaitNano = (sysClockNano + randNano());
				//if above 1 sec, add 1 to sec
				if ((processTable[planToSchedule].eventWaitNano > 1000000000)){
					processTable[planToSchedule].eventWaitSec++;
					processTable[planToSchedule].eventWaitNano -= 1000000000;

				}
			}else{
				//full time used
				printf("OSS: Worker %d PID %d finished with it's scheduled time.\n", planToSchedule, processTable[planToSchedule].pid);
				incrementByX(schTime);
				processTable[planToSchedule].serviceTimeNano += schTime;
				if ((processTable[planToSchedule].serviceTimeNano >= 1000000000)){
					processTable[planToSchedule].serviceTimeSec++;
					processTable[planToSchedule].serviceTimeNano -= 1000000000;

				}
			}
		}


//use logic to see if a new process could/should be forked.if so, set new nano to 1, if total has  launched, set a kill flag.(reset a flag at launch maybe.
		//if can create worker, create worker and update PCB:
// -t needs to pass, and worker simultaneous and max limits must not be passed
		if (totalNewWorkers < numWorkers){
			if (activeWorkers < workerLimit ){
//should also stop if more then 3 actual seconds have passed. is this from app start or since last worker has launched? assuming the first one. Not sure how to go about this just yet..
				childPid = fork();
				if (childPid == -1){
					printf("Fork Process Failed!\n");
					return EXIT_FAILURE;
				}
				//child side of the fork if
				if (childPid == 0) {
					int timeSec = randSeconds(time);
					int timeNano = randNano();
					char secArg[10];
					char nanoArg[10];
					sprintf(secArg, "%d", timeSec);
					sprintf(nanoArg, "%d", timeNano);
					char * args[] = {"./worker", secArg, nanoArg, NULL};
//worker is getting sent the wrong data..
					execvp("./worker", args);
				}
				//parent side of fork if
				totalNewWorkers++;
//				activeWorkers++;
				//update pcb entry after a fork:
				processTable[n].occupied = 1;
				processTable[n].pid = childPid;
				processTable[n].startSeconds = sysClockSec;
				processTable[n].startNano = sysClockNano;
//verify pcb is updated correctly here
				n++;
			}
		}else{
			termFlag2 = 1;
		}
	//increment time (tentitive location in the loop)
	incrementClock();


	//end loop
	}



//print system report
//need to see what all this should include

	//close output file:
	fclose(outputFile);
	//clear message ques:
	if (msgctl(msqid, IPC_RMID, NULL) == -1){
		perror("msgctl failed to get rid of que in parent ");
		exit(1);
	}
}
