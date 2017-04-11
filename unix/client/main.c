#include <stdio.h>
#include <stdlib.h>

#include <termios.h>
#include <math.h>
#include <strings.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/errno.h>


#define SHMSZ 32
#define KEY 2809
#define STDIN_FILENO 0


void cleanup();


double time_diff(struct timeval x, struct timeval y) // calculates the difference  between two given timesA
{
    double x_ms , y_ms , diff;
    
    x_ms = (double)x.tv_sec * 1000000 + (double)x.tv_usec;
    y_ms = (double)y.tv_sec * 1000000 + (double)y.tv_usec;
    
    diff = (double)y_ms - (double)x_ms;

    return fabs(diff);
}

/**
 *
 *  Keypress detection
 *
 */
int kbhit()
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;

    fd_set fds;
    FD_ZERO(&fds);

    FD_SET(STDIN_FILENO, &fds); 
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);

    return FD_ISSET(STDIN_FILENO, &fds);
}

/**
 *
 *  Allow nonblocking reading
 *
 */
void nonblock(int state)
{
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);  //- get the terminal state

    if (state == 1)
    {
        ttystate.c_lflag &= ~ICANON;   //- turn off canonical mode
        ttystate.c_cc[VMIN] = 1;       //- minimum input chars read
    }
    else if (state==0)
    {
       ttystate.c_lflag |= ICANON;     //- turn on canonical mode
    }
    //- set the terminal attributes.
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
}

// Struct for all the shared memory content
typedef struct 
{
    long number;             // Used to exchange input/slot numbers
    long factnumber;
    int testprocess;
    long slots[10];          // Array to store the slot variables
    char clientflag;        // Flag used to signal when number is ready
    char serverflags[10];   // Array to store the flag for each query
    int progress[10];      // Array to store the progress of each query
} SharedMem;

typedef struct
{
    int id;                 // Unique ID for the query
    long number;             // Original number for this query
    int slot;               // Slot used for this query
    struct timeval start;   // Time at which the query started
} Query;



void displayProgress(Query x[], int len, int width);

// Global variables
int shmid = 0;                 
SharedMem * data = NULL;   
Query Squid[10];  
int progress_last = 0;
int qID = 1;

int main()
{
    signal(SIGINT, cleanup);
    nonblock(1);
    struct timeval now, lastOp;
    double time_difference = 0;



    /**
     *
     *  This accesses the shared memory with the server
     *
     */

    if ((shmid = shmget(KEY, sizeof(SharedMem), 0666)) < 0)
    {
        printf("Error: Could not get shared memory. errno = %d\n", errno);
        exit(1);
    } printf("Shared memory id: %d\n", shmid);
    if ((data = (SharedMem *)shmat(shmid, NULL, 0)) == (void *)-1)
    {
        printf("Error: Could not get shared memory address.\n");
        exit(1);
    }

    gettimeofday(&lastOp, NULL);

    while(1)
    {
       if (kbhit()) //Checks for keypress
        {
            char buf[10];
            long num;

            
            bzero(buf, sizeof(buf));
            if (scanf("%s", &buf) > 0)//scans input
            {

                num = atol(buf);
                printf("%ld\n",num);
                if (!strcmp(buf, "quit") || !strcmp(buf, "q")) // if q or quit it will break the loop and call cleanup
                {

                    break;
                }

                if (num == 0 && strcmp(buf, "0") != 0) // check if user inputted a number
                {
                    printf("Error: That is not a number!\n");
                }
                else
                {
                    if (data->clientflag == 0 && data->testprocess == 0)
                    {

                        if(num == 0) data->testprocess = 1;
                        data->number = num; // assign the given number to the shared memory for the server to access
                        data->clientflag = 1; // set clientflag to 1 so the server knows to read from data->number

                        while (data->clientflag != 0) {} //wait for a response from the server

                        for (int i = 0; i < sizeof(Squid); i++) //this for loop is used to keep track of all queries being used
                        {
                            if (Squid[i].id == 0)
                            {
                                Squid[i].id = qID;
                                Squid[i].number = num;
                                Squid[i].slot = data->number;
                                qID++;
                                gettimeofday(&Squid[i].start, NULL);
                                printf("Query ID = %d\n", Squid[i].id); //assign the time the query was started
                                break;
                            }
                        }

                    }
                    else
                    {
                        printf("Error: The server is busy.\n");
                    }
                }
                gettimeofday(&lastOp, NULL); // get current time, relavent later on when looking for query start and finish difference
            }       
        }

        int FoundingFathers;

        for (int i = 0; i < 10; i++) // this forloop checks if a factorised number has been returned to the client
            {           
                if (data->serverflags[i] == 1)
                {
                    printf("Factors of %ld: ",data->factnumber);
                    printf("%ld\n", data->slots[i]);
                    data->serverflags[i] = 0;
                    FoundingFathers = 1;
                    progress_last = 0;
                }
            }

        for (int i = 0; i < 10; i++) // this for loop checks the progress of each query, if its 100% it will print out the query details
        {
        
            if (data->progress[i] == 100)
            {

                printf("Query complete for %ld.\n", Squid[i].number);
                gettimeofday(&now, NULL);
                double complete = time_diff(Squid[i].start, now);
                printf("Query completed in %lf\n", complete);
            
                // Reset the space used for that query
                data->progress[i] = 0;
                data->serverflags[i] = 0;
                Squid[i].id = 0;

                // Update last operation time
                gettimeofday(&lastOp, NULL);
                    
                
            }

        }

        gettimeofday(&now, NULL);
        time_difference = time_diff(now, lastOp); // in microseconds

        // If 500ms has elapsed since server response or user input
        if (time_difference > 500000)
        {
            //printf("timedif\n");
            // If the progress bar is already (and last) in the console
            if (progress_last)
            {
                // Move up and clear the line
                printf("\033[F\033[J");

                // If the progress bars move onto a second line
                int count = 0;
                for (int i = 0; i < sizeof(Squid)/sizeof(Query); i++)
                {
                    if (Squid[i].id != 0) count++;
                }
                if (count > 5) printf("\033[F\033[J");
            }
            // Display the progress bars
            displayProgress(Squid, sizeof(Squid)/sizeof(Query), 25);

            // Update the last operation time
            gettimeofday(&lastOp, NULL);
        }
    }
    cleanup();
    return 1;
}


void cleanup()
{
    data->clientflag = 2;
    if (shmdt(data) == -1) printf("Error: Could not detach shared memory.\n");



    printf("\nBye!\n");

    exit(0);
}


void displayProgress(Query x[], int len, int width)
{
    int i, q = 0, n = 100;
    for (i = 0; i < 10; i++)
    {
        // For each current query (ID not 0)
        if (x[i].id != 0)
        {
            // Calculuate the ratio of complete-to-incomplete.
            float ratio = data->progress[i] / (float)n;
            int   c     = ratio * width;

            // Show the percentage complete.
            printf("Q%d: %3d%% [", x[i].id, (int)(ratio * 100));

            // Show the load bar.
            int j;
            for (j = 0; j < c; j++)
               printf("=");

            for (j = c; j < width; j++)
               printf(" ");

            printf("] ");

            q++;
        }
    }

    // If there was at least one query
    if (q)
    {
        printf("\n");

        // Set that the progress bar was the last thing in the console
        progress_last = 1;
    }
}