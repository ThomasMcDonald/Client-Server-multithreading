

#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <Winsock2.h>
#pragma comment(lib, "WS2_32.lib")


#define SHMSZ 32
#define KEY 2809
#define STDIN_FILENO 0


//void cleanup();


double time_diff(struct timeval x, struct timeval y)
{
	double x_ms, y_ms, diff;

	x_ms = (double)x.tv_sec * 1000000 + (double)x.tv_usec;
	y_ms = (double)y.tv_sec * 1000000 + (double)y.tv_usec;

	diff = (double)y_ms - (double)x_ms;

	return fabs(diff);
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
HANDLE hMapFile;
TCHAR szName[] = TEXT("Global\MyFileMappingObject");
int main()
{
	//signal(SIGINT, cleanup);

	struct timeval now, lastOp;
	double time_difference = 0;



	hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,
		FALSE,
		szName);

	if (hMapFile == NULL)
	{
		printf("Could not open file mapping object (%d).\n",
			GetLastError());
		return 1;
	}

	data = (SharedMem *)MapViewOfFile(hMapFile, // handle to map object
		FILE_MAP_ALL_ACCESS,  // read/write permission
		0,
		0,
		sizeof(data));

	//gettimeofday(&lastOp, NULL);

	while (1)
	{
		if (_kbhit())
		{
			char buf[10];
			char * notincluded;
			long num;



			if (scanf("%s", &buf) > 0)
			{
				num = atol(buf);

				if (!strcmp(buf, "quit") || !strcmp(buf, "q"))
				{

					break;
				}

				if (num == 0 && strcmp(buf, "0") != 0)
				{
					printf("Error: That is not a number!\n");
				}
				else
				{
					printf("Client Flag: %d\n", data->clientflag);
					if (data->clientflag == 0 && data->testprocess == 0)
					{

						if (num == 0) data->testprocess = 1;
						data->number = num;
						data->clientflag = 1;

						printf("Client Flag: %d\n", data->clientflag);
						while (data->clientflag != 0) {}

						for (int i = 0; i < sizeof(Squid); i++)
						{
							if (Squid[i].id == 0)
							{
								Squid[i].id = qID;
								Squid[i].number = num;
								Squid[i].slot = data->number;
								qID++;
								//gettimeofday(&Squid[i].start, NULL);
								printf("Query ID = %d\n", Squid[i].id);
								break;
							}
						}

					}
					else
					{
						printf("Error: The server is busy.\n");
					}
				}
				//gettimeofday(&lastOp, NULL);
			}
		}

		int FoundingFathers;

		for (int i = 0; i < 10; i++)
		{
			if (data->serverflags[i] == 1)
			{
				printf("Factors of %ld: ", data->factnumber);
				printf("%ld\n", data->slots[i]);
				data->serverflags[i] = 0;
				FoundingFathers = 1;
				progress_last = 0;
			}
		}

		//if(FoundingFathers)
		//{
		//    gettimeofday(&lastOp, NULL);
		//}
		//if(data->testprocess != 1){         
		for (int i = 0; i < 10; i++)
		{
			//printf("%d\n",i );
			//if(data->progress[i] > 0)
			//  printf("%d\n",data->progress[i]);
			//printf("%d = %d\n",i,data->progress[i]);
			// If a request has finished

			if (data->progress[i] == 100)
			{
				//printf("100%% boi\n");
				// Find the query

				printf("Query complete for %ld.\n", Squid[i].number);
				//gettimeofday(&now, NULL);
				//				double complete = time_diff(Squid[i].start, now);
				//	printf("Query completed in %lf\n", complete);

				// Reset the space used for that query
				data->progress[i] = 0;
				data->serverflags[i] = 0;
				Squid[i].id = 0;

				// Update last operation time
				//gettimeofday(&lastOp, NULL);


			}

		}

		//gettimeofday(&now, NULL);
		//		time_difference = time_diff(now, lastOp); // in microseconds
		// printf("%ld\n",time_difference);
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
				for (int i = 0; i < sizeof(Squid) / sizeof(Query); i++)
				{
					if (Squid[i].id != 0) count++;
				}
				if (count > 5) printf("\033[F\033[J");
			}
			// Display the progress bars
			displayProgress(Squid, sizeof(Squid) / sizeof(Query), 25);

			// Update the last operation time
			//gettimeofday(&lastOp, NULL);
		}

		//  }
	}
//	cleanup();
	return 1;
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
			int   c = ratio * width;

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

