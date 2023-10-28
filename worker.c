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
#define SHMKEY 859048
#define BUFF_SZ sizeof ( int )
#define PERMS 0777
typedef struct msgbuffer {
	long mtype;
	char strData[100];
	int intData;
} msgbuffer;
// !!! add random number generator code: copy from oss.c  !!!

//worker's goals:
//do a loop:
	//check for message - blocking wait is fine - the message will contain the time the worker can posibly run for
	//pick option of "process" to run after message, the options are for how much of the time quantum should be used.
	//reply to the message sent by parent with the option chosen, aka, how long it actulally ran for.
	// if not terminated, it waits for the next message to be sent by oss

// time that the process actually uses if not all is randomly generated, so add pid seeded generator function
//process percentages are something like 90-95% for full time, 3-4% for io and 1-2% for termination

int main(int argc, char** argv){
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

		int argSec = atoi(argv[1]);
		int argNano = atoi(argv[2]);
		int timeUp = 0;

		//loop that checks the clock:
		while(timeUp != 1){
//loop pt1. check for message from parent w/possible time quantum:
			// message queue read:
			if (msgrcv(msqid, &buf, sizeof(msgbuffer), getpid(), 0) == -1) {
				perror("failed to recieve message form parent");
				exit(1);
			}
//loop pt2. after message is recieved, use logic to decide between the 3 options: full time run, partial io, and partial w/termination.

//loop pt3. respond to parrent with info related to the first
				//message back to parent:
				buf.mtype = getppid();
				buf.intData = 0;
				strcpy(buf.strData,"Some message back to Parent from Child\n");
				if (msgsnd(msqid, &buf, sizeof(msgbuffer)-sizeof(long),0) == -1) {
					perror("msgsnd to parent failed\n");
					exit(1);
				}
				printf("WORKER PID: %d PPID %d SysClockS: %d SysclockNano %d TermTimeS: %d TermTimeNano: %d --Terminating\n",getpid(),getppid(),cint[0], cint[1], timeoutSec, timeoutNano);
// end loop. if not terminated, loop back to wait for the next message from oss
		}
}
