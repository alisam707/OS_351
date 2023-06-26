#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include "msg.h"    /* For the message struct */
#include <cstring>

using namespace std;

/* The size of the shared memory segment */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid, msqid;

/* The pointer to the shared memory */
void *sharedMemPtr = NULL;


/**
 * The function for receiving the name of the file
 * @return - the name of the file received from the sender
 */
string recvFileName()
{
    /* The file name received from the sender */
    string fileName;
        
    /* Declare an instance of the fileNameMsg struct to hold the message received from the sender */
    struct fileNameMsg receivedMsg;

    /* Receive the file name using msgrcv() */
    if (msgrcv(msqid, &receivedMsg, sizeof(struct fileNameMsg) - sizeof(long), 0, 0) == -1) {
        perror("msgrcv");
        exit(1);
    }
    
    /* Store the received file name */
    fileName = receivedMsg.fileName;

    /* Return the received file name */
    return fileName;
}

//unsigned long mainLoop(const char* fileName);
unsigned long mainLoop(const char* fileName)
{
    /* Open the file for writing */
    FILE* fp = fopen(fileName, "wb");
    if (!fp) {
        perror("fopen");
        exit(1);
    }

    /* The buffer to store data received from shared memory */
    char buffer[SHARED_MEMORY_CHUNK_SIZE];

    /* The received message structure */
    message receivedMsg;

    /* The acknowledgement message structure */
    ackMessage ackMsg;

    while (true) {
        /* Wait to receive a message from the sender */
        if (msgrcv(msqid, &receivedMsg, sizeof(receivedMsg.size), SENDER_DATA_TYPE, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }

        /* Get the size of the received chunk */
        int dataSize = receivedMsg.size;

        if (dataSize > 0) {
            /* Read the data from shared memory */
            memcpy(buffer, sharedMemPtr, dataSize);

            /* Write the data to the file */
            if (fwrite(buffer, sizeof(char), dataSize, fp) != dataSize) {
                perror("fwrite");
                exit(1);
            }

            /* Send acknowledgement message */
            ackMsg.mtype = RECV_DONE_TYPE;
            if (msgsnd(msqid, &ackMsg, sizeof(ackMsg), 0) == -1) {
                perror("msgsnd");
                exit(1);
            }
        } else {
            /* Close the file */
            fclose(fp);
            break;
        }
    }

    /* Calculate the number of bytes received */
    unsigned long numBytesReceived = ftell(fp);

    return numBytesReceived;
}


/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param msqid - the id of the shared memory
 * @param sharedMemPtr - the pointer to the shared memory
 */
void init(int& shmid, int& msqid, void*& sharedMemPtr)
{
    /* Create a file called keyfile.txt containing string "Hello world" */

    /* Generate the key */
    key_t key = ftok("keyfile.txt", 'a');
    if (key == -1) {
        perror("ftok");
        exit(1);
    }

    /* Allocate a shared memory segment */
    shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    /* Attach to the shared memory */
    sharedMemPtr = shmat(shmid, NULL, 0);
    if (sharedMemPtr == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    /* Create a message queue */
    msqid = msgget(key, IPC_CREAT | 0666);
    if (msqid == -1) {
        perror("msgget");
        exit(1);
    }
}

/**
 * Performs cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */
void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr)
{
    /* Detach from shared memory */
    shmdt(sharedMemPtr);

    /* Deallocate the shared memory segment */
    shmctl(shmid, IPC_RMID, NULL);

    /* Deallocate the message queue */
    msgctl(msqid, IPC_RMID, NULL);
}

/**
 * Handles the exit signal
 * @param signal - the signal type
 */
void ctrlCSignal(int signal)
{
    /* Free System V resources */
    cleanUp(shmid, msqid, sharedMemPtr);
    exit(0);
}


int main(int argc, char** argv)
{
    /* Install a signal handler */
    signal(SIGINT, ctrlCSignal);
            
    /* Initialize */
    init(shmid, msqid, sharedMemPtr);
    
    /* Receive the file name from the sender */
    string fileName = recvFileName();
    
    /* Go to the main loop */
    fprintf(stderr, "The number of bytes received is: %lu\n", mainLoop(fileName.c_str()));

    /* Detach from shared memory segment, and deallocate shared memory and message queue */
    cleanUp(shmid, msqid, sharedMemPtr);
    
    return 0;
}