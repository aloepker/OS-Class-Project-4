//Written by Adam Spencer Loepker
//Finished on ???
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#define PERMS 0777
typedef struct msgbuffer {
	long mtype;
//	char strData[100];
	int intData;
} msgbuffer;
		
//correct RNGs
		
//random number generator for worker's task:
int randOption(){
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);

	srand((unsigned int)ts.tv_nsec);
	return rand()%100;
}
//random number generator for workers time quantum used:
int randTime(int max){
	srand(getpid()+1);
	return 1+rand()%max;
}

int main(int argc, char** argv){
	int rNum;
	int timeUsed;
	msgbuffer buf;
	buf.mtype = 1;
	int msqid = 0;
	key_t key;
	// aquire key for message queue
	if ((key = ftok("msgq.txt", 1)) == -1) {
		perror("child ftok error");
		exit(1);
	}
	// create message queue
	if ((msqid = msgget(key, PERMS)) == -1) {
		perror("msgget error in child");
		exit(1);
	}
	printf("Child has access to the message que!\n");

		int timeoutSec = atoi(argv[1]);
		int timeoutNano = atoi(argv[2]);
		int timeUp = 0;

//test output file:
char fileName[20];
snprintf(fileName, sizeof(fileName), "debugWorker%d", getpid());
FILE * output = fopen(fileName,"w");
if (output == NULL){
	printf("Debug File Error!\n");
	return 1;
}
		//loop that checks the clock time sent:
		while(timeUp != 1){
		//loop pt1. check for message from parent w/possible time quantum:
			// message queue read:

			if (msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) {
printf("worker: %d  parent process id is : %d\n", getpid(), getppid());
				perror("WTF... failed to recieve message form parent");
				exit(1);
			}
			//after messaage is recieved, use logic to decide between the 3 options: full time run, partial io, and partial w/termination.
	
			rNum = randOption();
fprintf(output,"%d\n",rNum);
	
			if ((rNum < 10)){
			//terminate if < 7
				timeUsed = -randTime(buf.intData);
				printf("WORKER PID: %d PPID %d TermTimeS: %d TermTimeNano: %d --Terminating\n",getpid(),getppid(), timeoutSec, timeoutNano);
				timeUp = 1;
			}else if((rNum < 20)){
			//io opperation if < 15
				timeUsed = randTime(buf.intData);
			}else{
			//full time quantum used
				timeUsed = buf.intData;
			}
				//message back to parent:
				buf.mtype = getppid();
				buf.intData = timeUsed;
				if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long),0) == -1) {
					perror("msgsnd to parent failed\n");
					exit(1);
				}
		}
fclose(output);
}
