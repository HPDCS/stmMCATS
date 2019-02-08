/* =============================================================================
 *
 * vacation.c
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of ssca2, please see ssca2/COPYRIGHT
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 * 
 * ------------------------------------------------------------------------
 * 
 * Unless otherwise noted, the following license applies to STAMP files:
 * 
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */


#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include "map.h"
#include "memory.h"
#include "random.h"
#include "thread.h"
#include "timer.h"
#include "tm.h"
#include "types.h"
#include "utility.h"

#define TX_CLASS_NUMBER 1

enum param_types {
    PARAM_THREADS      = (unsigned char)'c',
    PARAM_WRITES       = (unsigned char)'n',
    PARAM_TRANSACTION_LENGTH      = (unsigned char)'q',
    PARAM_DATA_ITEMS    = (unsigned char)'r',
    PARAM_TRANSACTIONS = (unsigned char)'t',
    PARAM_ACCESSED_DATA_ITEMS         = (unsigned char)'u',
};

#define PARAM_DEFAULT_THREADS      (1)
#define PARAM_DEFAULT_WRITES       (0)
#define PARAM_DEFAULT_TRANSACTION_LENGTH      (90)
#define PARAM_DEFAULT_DATA_ITEMS    (1000)
#define PARAM_DEFAULT_TRANSACTIONS (1 << 26)
#define PARAM_DEFAULT_ACCESSED_DATA_ITEMS         (0)

double global_params[256]; /* 256 = ascii limit */
long numThread;
long numTransaction;
long transactionLength;
int writesPercentage;
int accessedPercentage;
long numDataItems;
long *data_items;
volatile int stop=0;


/* =============================================================================
 * displayUsage
 * =============================================================================
 */
static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    printf("    c <UINT>   Number of threads                   (%i)\n",
           PARAM_DEFAULT_THREADS);
    printf("    n <UINT>   Percentage of writes  (%i)\n",
           PARAM_DEFAULT_WRITES);
    printf("    q <UINT>   Transaction length   (%i)\n",
           PARAM_DEFAULT_TRANSACTION_LENGTH);
    printf("    r <UINT>   Number of data items		       (%i)\n",
           PARAM_DEFAULT_DATA_ITEMS);
    printf("    t <UINT>   Number of tansactions              (%i)\n",
           PARAM_DEFAULT_TRANSACTIONS);
    printf("    u <UINT>   Percentage of accessed data items     (%i)\n",
           PARAM_DEFAULT_ACCESSED_DATA_ITEMS);
    exit(1);
}


/* =============================================================================
 * setDefaultParams
 * =============================================================================
 */
static void
setDefaultParams ()
{
    global_params[PARAM_THREADS]      = PARAM_DEFAULT_THREADS;
    global_params[PARAM_WRITES]       = PARAM_DEFAULT_WRITES;
    global_params[PARAM_TRANSACTION_LENGTH]      = PARAM_DEFAULT_TRANSACTION_LENGTH;
    global_params[PARAM_DATA_ITEMS]    = PARAM_DEFAULT_DATA_ITEMS;
    global_params[PARAM_TRANSACTIONS] = PARAM_DEFAULT_TRANSACTIONS;
    global_params[PARAM_ACCESSED_DATA_ITEMS]         = PARAM_DEFAULT_ACCESSED_DATA_ITEMS;
}

/* =============================================================================
 * parseArgs
 * =============================================================================
 */
static void
parseArgs (long argc, char* const argv[])
{
    long i;
    long opt;

    opterr = 0;

    setDefaultParams();

    while ((opt = getopt(argc, argv, "c:n:q:r:t:u:")) != -1) {
        switch (opt) {
            case 'c':
            case 'n':
            case 'q':
            case 'r':
            case 't':
            case 'u':
                global_params[(unsigned char)opt] = atol(optarg);
                break;
            case '?':
            default:
                opterr++;
                break;
        }
    }

    for (i = optind; i < argc; i++) {
        fprintf(stderr, "Non-option argument: %s\n", argv[i]);
        opterr++;
    }

    if (opterr) {
        displayUsage(argv[0]);
    }
}

void
client_run (void* argPtr)
{

    TM_THREAD_ENTER();

    long myId = thread_getId();
    random_t* randomPtr;
    long executed_tx=0;
   // client_t* clientPtr = ((client_t**)argPtr)[myId];

    long i;
    int read;
    while(!stop) {
       // long r = random_generate(randomPtr) % 100;
	int l;
	long item;
 	TM_BEGIN();
	for (l=0; l<transactionLength; l++) {
	    item=rand() % ((numDataItems*accessedPercentage)/100)+1;
	    if ((rand() % 100) > writesPercentage) {
	    	TM_SHARED_READ(data_items[item]);
		//printf("\nitem: %ld, type READ", item);
	    } else {
		TM_SHARED_WRITE(data_items[item],0);
		//printf("\nitem: %ld, type WRITE", item);
	    }

	}
        TM_END();

	if (myId==0) {
	    executed_tx++;
		//accessedPercentage =50+50*sin(6.283 * (float)(executed_tx)/(float)numTransaction);
		//if (accessedPercentage==0) accessedPercentage++;
		//printf("\nexecuted_tx %ld, accessedPercentage %i", executed_tx, accessedPercentage);
	    if(executed_tx==numTransaction) {
		stop=1;	
		break;
	    }
	} else {
	    if(stop) break;
	}

    }

    TM_THREAD_EXIT();
}

/* =============================================================================
 * main
 * =============================================================================
 */
MAIN(argc, argv)
{

    TIMER_T start;
    TIMER_T stop;

    GOTO_REAL();

    /* Initialization */
    setenv("STM_STATS","1",0);
    parseArgs(argc, (char** const)argv);
    SIM_GET_NUM_CPU(global_params[PARAM_THREADS]);
    numThread = global_params[PARAM_THREADS];
    numTransaction = (long)global_params[PARAM_TRANSACTIONS];
    transactionLength = (long)global_params[PARAM_TRANSACTION_LENGTH];	
    writesPercentage = (long)global_params[PARAM_WRITES];
    accessedPercentage = (long)global_params[PARAM_ACCESSED_DATA_ITEMS];
    numDataItems = (long)global_params[PARAM_DATA_ITEMS];
    data_items= (long *) malloc(sizeof(long) * numDataItems);


    TM_STARTUP(numThread);

    P_MEMORY_STARTUP(numThread);
    thread_startup(numThread);

    /* Run transactions */
    //printf("Running threads... ");
    //fflush(stdout);
    TIMER_READ(start);
    GOTO_SIM();
#ifdef OTM
#pragma omp parallel
    {
        client_run();
    }
#else
    thread_start(client_run, (void*)NULL);
#endif
    GOTO_REAL();
    TIMER_READ(stop);
    //puts("done.");
    printf("Threads: %i\tElapsed time: %f",numThread, TIMER_DIFF_SECONDS(start, stop));
    fflush(stdout);
   
    TM_SHUTDOWN();
	if (getenv("STM_STATS") != NULL) {
		unsigned long u;
		if (stm_get_global_stats("global_nb_commits", &u) != 0){
			printf("\tThroughput: %f\n",u/TIMER_DIFF_SECONDS(start, stop));
		}
	}

    P_MEMORY_SHUTDOWN();

    GOTO_SIM();

    thread_shutdown();

    MAIN_RETURN(0);
}


/* =============================================================================
 *
 * End
 * =============================================================================
 */
