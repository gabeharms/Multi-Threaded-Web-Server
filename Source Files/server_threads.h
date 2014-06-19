

#include <smsa_threads.c>

//
// Type Definitions


//
// Funtional Prototypes

int setupThreads ( MY_THREAD * backlog, int max );
int findFreeThread ( MY_THREAD * backlog, int max );
int waitForThreads ( MY_THREAD *backlog, int max );
int areThreadsMaxedOut ( int max );


