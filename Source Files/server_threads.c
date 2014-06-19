////////////////////////////////////////////////////////////////////////////////
//
//  File          : server_threads.c
//  Description   : The methods here handle all of the thread management and manipulation.
//
//   Author        : Gabe Harms
//   Last Modified : Mon Oct 28 06:58:31 EDT 2013
//

#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

// Project Include Files
#include <smsa.h>
#include <smsa_network.h>
#include <cmpsc311_log.h>

// Global Variables
char *colors[] = { RED, PURPLE, ORANGE, GREEN };
int threadCount = 0;



int setupThreads ( MY_THREAD * backlog, int max ) {
	
	for (int i =0; i < max; i++) {
                backlog[i].available = 1;
                backlog[i].color = colors[i];
        }
	
	return 0;	
}

int findFreeThread ( MY_THREAD * backlog, int max ) {

	int usable_index = 0;

	while ( !backlog[usable_index].available ) { usable_index++; }

        backlog[usable_index].available = 0;

        logMessage ( LOG_INFO_LEVEL, "Request will be handled by the number %d thread", usable_index+1 );

        threadCount++;

	return usable_index;

}

int waitForThreads ( MY_THREAD *backlog, int max ) {

	
	logMessage ( LOG_INFO_LEVEL, "Thread Backlog full. Releasing all threads now" );
        for ( int i =0; i < max; i++ ) {
        	if ( !backlog[i].available ) {
                	pthread_join ( backlog[i].thread, NULL );
                        backlog[i].available = 1;
                }
        }
        threadCount = 0;

	return 0;

}

int areThreadsMaxedOut ( int max ) {

	return (threadCount < max );

}
