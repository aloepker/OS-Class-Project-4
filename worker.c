//Written by Adam Spencer Loepker
//Finished on October 14th, 2023
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

#define SHMKEY 859048
#define BUFF_SZ sizeof ( int )
#define PERMS 0777
typedef struct msgbuffer {
	long mtype;
	char strData[100];
	int intData;
} msgbuffer;

//signal handler:
void signal_handler(int signum){
printf("60 Second time limit Reached! Terminating Worker Program\n");
exit(0);
}

//worker's goals:
//do a loop:
	//check for message - blocking wait is fine - the message will contain the time the worker can posibly run for
	//pick option of "process" to run after message, the options are for how much of the time quantum should be used.
	//reply to the message sent by parent with the option chosen, aka, how long it actulally ran for.
	// if not terminated, it waits for the next message to be sent by oss
//end loop

// time that the process actually uses if not all is randomly generated, so add pid seeded generator function
//process percentages are something like 90-95% for full time, 3-4% for io and 1-2% for termination

int main(int argc, char** argv){
	//60 second countdown to termination:
	signal(SIGALRM, signal_handler);
	alarm(60);
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

	if(argc>2){
	//attach to shared memory:
		int shmidc = shmget(SHMKEY, BUFF_SZ, 0777);
		if (shmidc == -1){
			perror("Child process shared memory error!");
			return EXIT_FAILURE;
		}
		int * cint= (int*)(shmat(shmidc,0,0));
		int argSec = atoi(argv[1]);
		int argNano = atoi(argv[2]);
		int timeoutSec = argSec + cint[0];
		int timeoutNano = argNano + cint[1];
		int timeUp = 0;
		int startSec = cint[0];
		int secActive = 0;
		printf("WORKER PID: %d PPID %d SysClockS: %d SysclockNano %d TermTimeS: %d TermTimeNano: %d --Just Starting\n",getpid(),getppid(),cint[0], cint[1], timeoutSec, timeoutNano);
		//loop that checks the clock:
		while(timeUp != 1){
			// message queue read:
			if (msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) {
				perror("failed to recieve message form parent");
				exit(1);
			}
			//time check:
			if ( secActive < (cint[0]-startSec) ){
				secActive++;
				// "second" output
				printf("WORKER PID: %d PPID %d SysClockS: %d SysclockNano %d TermTimeS: %d TermTimeNano: %d -- %d seconds have passed since starting\n",getpid(),getppid(),cint[0], cint[1], timeoutSec, timeoutNano, secActive);
			}

			//termination condition:
			if((timeoutSec == cint[0] && timeoutNano < cint[0]) || (timeoutSec < cint[0])) {
				timeUp = 1;
				//termination message back to parent:
				buf.mtype = getppid();
				buf.intData = 0;
				strcpy(buf.strData,"Termination  message back to Parent from Child\n");
				if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long),0) == -1) {
					perror("msgsnd to parent failed\n");
					exit(1);
				}
				printf("WORKER PID: %d PPID %d SysClockS: %d SysclockNano %d TermTimeS: %d TermTimeNano: %d --Terminating\n",getpid(),getppid(),cint[0], cint[1], timeoutSec, timeoutNano);
			}else{
				//send nontermination message to parent here:
				buf.mtype = getppid();
				buf.intData = 1;
				strcpy(buf.strData,"Nontermination message back to Parent from Child\n");
				if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long),0) == -1) {
					perror("msgsnd to parent failed\n");
					exit(1);
				}
			}
		}
	} else {
		printf("incorrect number of arguments\n");
	}
}
