#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>
#include <math.h>

#define KEY 2809


void cleanup();
void * threadable(void * input);

typedef struct 
{
    pthread_mutex_t * mutex;
    pthread_cond_t * cond;
    int numberofkeys;
}semaphore;

semaphore * sem_get(int value) // initializes all semaphore struct variables.
{
    semaphore * sem = (semaphore *)malloc(sizeof(semaphore));

    sem->mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t)); 
    pthread_mutex_init(sem->mutex, NULL);

    sem->cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t)); 
    pthread_cond_init(sem->cond, NULL);

    sem->numberofkeys = value;

    return sem;
}


void sem_wait(semaphore * sem)//locks the mutexs in the semaphore struct
{
    pthread_mutex_lock(sem->mutex);
    while(sem->numberofkeys <= 0)
        pthread_cond_wait(sem->cond,sem->mutex);
    
    sem->numberofkeys -= 1;

    pthread_mutex_unlock(sem->mutex);
}

void sem_signal(semaphore * sem)//signals the sem_wait to unlock
{
    pthread_mutex_lock(sem->mutex);
    sem->numberofkeys += 1;
    pthread_cond_broadcast(sem->cond);
    pthread_mutex_unlock(sem->mutex);
}


typedef struct 
{
    long number;             // Used to exchange input/slot numbers
    long factnumber;
    int testprocess;
    long slots[10];          // Array to store the slot variables
    char clientflag;        // Flag used to signal when number is ready
    char serverflags[10];   // Array to store the flag for each query
    int progress[10];     
} SharedMem;

typedef struct
{
    int slotNumber;
    int flag;
    long number;
    int test;
    int progress;
}queries;

semaphore * mainQ;
semaphore * s_slots[10];
int id = 0;                     
int num_of_threads = 32 * 10; 
int threadsperQ;
pthread_t * threads = NULL;   
SharedMem * data = NULL;      
queries *query = NULL;  

//
int main(int argc, char *argv[])
{
    signal(SIGINT, cleanup);
    mainQ = sem_get(0);

    if (argc == 2 && atoi(argv[1]) > 0) num_of_threads = atoi(argv[1]) * 10;
        printf("Thread pool size set: %d\nThreads per request: %d\n", num_of_threads, num_of_threads / 10);


    for(int i=0;i<10;i++)
    {
        s_slots[i] = sem_get(1);
    }


      /**
     *
     *  This accesses the shared memory with the client
     *
     */

    if ((id = shmget(KEY, sizeof(SharedMem), IPC_CREAT | 0666)) < 0)
    {
        printf("Error: Could not get shared memory.\n");
        exit(1);
    } printf("Shared memory id: %d\n\n", id);
    if ((data = (SharedMem *)shmat(id, 0, 0)) == (void *)-1)
    {
        printf("Error: Could not get shared memory address.\n");
        exit(1);
    }

    threads = (pthread_t *)malloc(sizeof(pthread_t) * num_of_threads); // intialize enough memory for the threads
    for(int i=0;i<num_of_threads;i++)
        pthread_create(&threads[i],NULL,threadable,NULL);


    query = (queries *)malloc(sizeof(queries)); // intialize memory for the queries

    while (1)
    {
        if (data->clientflag > 1 || data->clientflag < 0) // if client flag is greater than 1 or lower than 0 break the loop
        {
            break;
        }

        if (data->clientflag == 1)
        {
                long facnum = data->number; 
                data->clientflag = 0;
                

                for(int i=0;i<10;i++) // loop through all the slots in shared memory 
                {
                    if(data->slots[i] == 0 && data->serverflags[i] != 1)
                    {
                         if(facnum==0) threadsperQ = 3; // if the number is 0 then threadspreQ is set to 3, 3 sets of 10
                         else threadsperQ = num_of_threads / 10;    
                        for(int j=0;j<threadsperQ;j++) //using the threadsperQ created above loop through and rotate numbers and pass them to the thread
                        {
                            for(int x = 0;x<num_of_threads;x++)
                            {
                                if(query[x].flag == 0)
                                {

                                    query[x].slotNumber = i;
                                    if (facnum == 0)
                                    {
                                    query[x].number = j * 10;
                                    query[x].test = 1;

                                    }
                                    else{
                                        if(j == 0)
                                            query[x].number = facnum;
                                        else
                                        {
                                            query[x].number = labs((facnum >> j) | (facnum << threadsperQ - j)); 
                                        }
                                    }
                                query[x].flag = 1;
                                sem_signal(mainQ);
                                break;
                                }
                            }
                        }
                        break;
                    }
            }
        } 
        for (int i = 0; i < 10; i++)
        {
            int threads = 0, percent = 0;
            for (int j = 0; j < num_of_threads; j++)
            {
                // If the job has been accepted by a thread
                if (query[j].flag < 2)
                    continue;

                // Sum the progress of jobs related to this slot
                if (query[j].slotNumber == i && data->serverflags[i] == 1)
                {
                    percent += query[j].progress;
                    threads++;
                }
            }

            if(num_of_threads > 0 && threadsperQ !=3 && data->serverflags[i] == 1)
            {
                data->progress[i] = ceil((int)((float)percent / (float) 32));
            }

        }
    }
    cleanup();
    return 1;
}



/**
 *
 * This function handles most of the manipulation of the data, it factorizes it and assigns it back to the shared memory for the
 * client to read from, it will also call sem_wait, sem_signal so multiple threads dont access the shared memory at the exact same time.
 */
void * threadable(void * input) 
{
    while(1)
    {
        sem_wait(mainQ);
        int jobnumber;
        int slotNumber;
        int qfound;
        for(int i=0;i<num_of_threads;i++)
        {
            if(query[i].flag == 1)
            {
                query[i].flag = 2;
                jobnumber = i;
                slotNumber = query[i].slotNumber;
                qfound = 1;
                break;
            }
        }
        if(qfound)
            {
                if (query[slotNumber].test == 1) // If the client sent a 0 the program would have turn on test mode and checks here
                {
                    for (int i = query[jobnumber].number; i < query[jobnumber].number + 10; i++)
                    {
                       sem_wait(s_slots[slotNumber]);
                        data->factnumber = 0;
                        data->slots[slotNumber] = i;
                        data->serverflags[slotNumber] = 1;

                        while (data->serverflags[slotNumber] != 0) {}

                        sem_signal(s_slots[slotNumber]);
                    }
                    if(query[jobnumber].number == 20) // if its up to 20 that means its on the last thread set, so test is no longer active and the server flag for that specific thread is stopped
                        {
                            data->testprocess = 0; 
                            data->serverflags[slotNumber] = 0;
                            data->progress[slotNumber] = 100;
                        }
                }
                else {
                    for (long i = 1; i <= query[jobnumber].number; i++)
                        {
                                if (query[jobnumber].number % i == 0)
                                {
                                    sem_wait(s_slots[slotNumber]);
                                    data->factnumber = query[jobnumber].number;
                                    data->slots[slotNumber] = i;
                                    data->serverflags[slotNumber] = 1;

                                    while (data->serverflags[slotNumber] != 0) {}

                                    sem_signal(s_slots[slotNumber]);
                                }

                                if(i == query[jobnumber].number)
                                {
                                    query[jobnumber].progress = 100;
                                    printf("Factors found for %ld\n", query[jobnumber].number);
                                    break;
                                }
                                else
                                {
                                     query[jobnumber].progress = ceil((int)((float)i / (float)query[jobnumber].number * 100));
                                }
                        }

                    }
                }
            
            query[jobnumber].flag = 3;

        }
}


void cleanup() //removes the shared memory
{
    
    if (shmdt(data) == -1) printf("Error: Could not detach shared memory.\n");
    if (shmctl(id, IPC_RMID, NULL) == -1 ) printf("Error: Failed to mark shared memory segment for destruction.\n");


    printf("\nBye!\n");


    exit(0);
}