#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <iostream> //added
#include "msg.h"    //for message struct

using namespace std;

//size of shared memory segment
#define SHARED_MEMORY_CHUNK_SIZE 1000

//id for shared memory segment
int shmid;

//id for message queue
int msqid;

//pointer to shared memory 
void *sharedMemPtr = NULL;

const char recvFileNamearray[] = "recvfile";

//receives name of file
string recvFileName()
{
	//file name received from snder
	string fileName;

	//holds message received from sender
	fileNameMsg msg;

    //receive file name (msgrcv())
	if(msgrcv(msqid, &msg, sizeof(fileNameMsg) - sizeof(long), FILE_NAME_TRANSFER_TYPE, 0) < 0)
	{
		perror("msgsnd");
		exit(-1);
	}
	
	//return received file name
	fileName = msg.fileName;
    return fileName;
}

void init(int& shmid, int& msqid, void*& sharedMemPtr)
{
	//generates key 
	key_t key = ftok("keyfile.txt", 'a');

	//checks if key was generated properly 
	if(key == -1)
	{
		perror("ftok");
		exit(1);
	}

	//allocates shared memory segment
	shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0644);

	if(shmid == -1)
	{
		perror("shmget");
		exit(1);
	}

	//attaches to shared memory 
	sharedMemPtr = shmat(shmid, (void*)0, 0);

	if(sharedMemPtr == (void*) -1)
	{
		perror("shmat");
		exit(1);
	}

	//creates message queue
	msqid = msgget(key, 0666);

	if(msqid == -1)
	{
		perror("msgget");
		exit(1);
	}
}

void mainLoop()
{
	//message received from sender
	int msgSize = 0;
	
	//open file for writing
	FILE* fp = fopen(recvFileNamearray, "w");

	//number of received bytes
	int numBytesRecv = 0;

	//filename received from sender
	string recvFileNameStr = recvFileNamearray;

	//append __recv at end of file
	recvFileNameStr.append("__recv");

	//checks for erorrs
	if(!fp)
	{
		perror("fopen");
		exit(-1);
	}

	message sndMsg;
	message rcvMsg;
	printf("...created message to store info...\n");
	msgSize = 1;

	//receives until sender sets to 0 (no more data to send) 
	while(msgSize != 0)
	{
		if(msgrcv(msqid, &rcvMsg, sizeof(rcvMsg) - sizeof(long), SENDER_DATA_TYPE, 0) == -1)
		{
			perror("msgrcv");
			exit(1);
		}

		msgSize = rcvMsg.size;
		printf("...recieved!\n");
		
		printf("DEBUG: Message Size %d Message Type %ld\n", rcvMsg.size, rcvMsg.mtype);
		printf("DEBUG: Message Struct Size %lu\n", sizeof(rcvMsg) - sizeof(long));

		/* If the sender is not telling us that we are done, then get to work */
		if(msgSize != 0)
		{
			//save shared memory to file
			if(fwrite(sharedMemPtr, sizeof(char), msgSize, fp) == 0)
			{
				perror("fwrite");
			}

			//tells sender receiver is ready for next set of bytes
			sndMsg.mtype = RECV_DONE_TYPE;
			sndMsg.size = 0;
			printf("Sending empty message back...\n");
			if(msgsnd(msqid, &sndMsg, 0, 0) == -1)
			{
				perror("msgsnd");
			}

			printf("...sent!\n");
		}
		else
		{
			printf("Empty file recieved. Closing file...\n");

			//closes file
			fclose(fp);
		}
	}
}

void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr)
{
	//detach from shared memory 
	if(shmdt(sharedMemPtr) < 0)
	{
		perror("shmdt");
		exit(-1);
	}
	
	//deallocate shared memory segment
	if(shmctl(shmid, IPC_RMID, NULL) < 0)
	{
		perror("shmctl");
		exit(-1);
	}
	
	//deallocate message queue 
	if(msgctl(msqid, IPC_RMID, NULL) < 0)
	{
		perror("msgtcl");
		exit(-1);
	}
}

//exit signal
void ctrlCSignal(int signal)
{
	//free system v resources
	cleanUp(shmid, msqid, sharedMemPtr);
}

int main(int argc, char** argv)
{

	//deletes message queue and shared memory segment before exit
	signal(SIGINT, ctrlCSignal);

	//initialize
	init(shmid, msqid, sharedMemPtr);

	mainLoop();

	//detach from shared memory segment and deallocate shared memory and message queue
	cleanUp(shmid, msqid, sharedMemPtr);

	return 0;
}
