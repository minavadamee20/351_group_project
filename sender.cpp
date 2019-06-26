#include <sys/shm.h>
#include <sys/msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <iostream>
#include "msg.h"    //for message struct

//size of shared memory segment
#define SHARED_MEMORY_CHUNK_SIZE 1000

//id for shared memory segment
int shmid; 

//id for message queue
int msqid;

//pointer to shared memory
void* sharedMemPtr;

void init(int& shmid, int& msqid, void*& sharedMemPtr)
{
	//generates key 
	key_t key = ftok("keyfile.txt", 'a');
	if(key < 0)
	{
		perror("ftok");		//if the generated key fails, it returns -1.
		exit(-1);
	}

	//gets id of shared memory segment
	shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, 0666 |IPC_CREAT);
	if(shmid < 0)
	{
		perror("shmget");
		exit(1);
	}

	//attach to shared memory 
	sharedMemPtr = (char*) shmat(shmid, NULL, 0);

	if(sharedMemPtr == (void*) -1)
	{
		perror("shmat");
		exit(-1);
	}
	
	//attach to message queue
	msqid = msgget(key, 0666 | IPC_CREAT);
	
	if(msqid < 0)
	{
		perror("msgget");
		exit(-1);
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
}

unsigned long sendFile(const char* fileName)
{

	//open file to read from 
	FILE* fp = fopen(fileName, "r");

	//buffer to store message sent to receiver
	message sndMsg;

	//buffer to store message received from receiver
	ackMessage rcvMsg;

	//number of sent bytes
	unsigned long numBytesSent = 0;

	//checks if file was opened
	if(!fp)
	{
		perror("fopen");
		exit(-1);
	}

	//reads through file
	while(!feof(fp))
	{
		if((sndMsg.size = fread(sharedMemPtr, sizeof(char), SHARED_MEMORY_CHUNK_SIZE, fp)) < 0)
		{
			perror("fread");
			exit(-1);
		}

		printf("starting sender program.. \n");

		//count numnber of sent bytes
		numBytesSent += sndMsg.size;

 		//send message to receiver saying data is ready to be read
		sndMsg.mtype = SENDER_DATA_TYPE;

		if(msgsnd(msqid, &sndMsg, sizeof(message) - sizeof(long), 0) < 0)
		{
			perror("msgsnd");
			exit(-1);
		}

 		//wait until receiver sends message of RECV_DONE_TYPE saying it finished saving chunk of memory
		do
		{
			msgrcv(msqid, &rcvMsg, sizeof(ackMessage) - sizeof(long), RECV_DONE_TYPE, 0);
		} while(rcvMsg.mtype != RECV_DONE_TYPE);
	}
	
	//nothing more to send, set size to 0 
	sndMsg.size = 0;
	if(msgsnd(msqid, &sndMsg, sizeof(message) - sizeof(long), 0) < 0)
	{
		perror("msgsnd");
		exit(-1);
	}

	//close file
	fclose(fp);

	return numBytesSent;
}

void sendFileName(const char* fileName)
{
	//length of filename
	int fileNameSize = strlen(fileName);

	//checks filename length to buffer maximum 
	if(fileNameSize > (sizeof(fileNameMsg)-sizeof(long)))
	{
		perror("fileNameSize too long");
		exit(-1);
	}

	//contains name of file 
	fileNameMsg msg;
	
	//sets message type FILE_NAME_TRANSFER_TYPE
	msg.mtype = FILE_NAME_TRANSFER_TYPE;
	
	//sets filename in message
	strncpy(msg.fileName, fileName, fileNameSize+1);

	//send message
	if(msgsnd(msqid, &msg, sizeof(msg) - sizeof(long), 0) < 0)
	{
		perror("msgsnd");
		exit(-1);
	}
}

int main(int argc, char** argv)
{

	//checks for arguments in command line
	if(argc < 2)
	{
		fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]);
		exit(-1);
	}

	//connect to shared memory and message queue
	init(shmid, msqid, sharedMemPtr);

	//send name of file
    	sendFileName(argv[1]);

	//send file 
	fprintf(stderr, "The number of bytes sent is %lu\n", sendFile(argv[1]));

	//cleansup 
	cleanUp(shmid, msqid, sharedMemPtr);

	return 0;
}
