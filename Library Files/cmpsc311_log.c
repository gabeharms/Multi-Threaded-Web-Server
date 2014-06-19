////////////////////////////////////////////////////////////////////////////////
//
//  File          : cmpsc311_log.c
//  Description   : This is the logging service for the CMPSC311 utility
//                  library.  It provides access enable log events,
//                  whose levels are registered by the calling programs.
//
//  Author   : Patrick McDaniel
//  Created  : Sat Sep 14 10:19:45 EDT 2013
//

// System include files
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>

// Project Include Files
#include <cmpsc311_log.h>

//
// Global data

unsigned long logLevel; 			// This is the current log level
char *descriptors[MAX_LOG_LEVEL];	// This is the array of log level descriptors
const char *logFilename; 			// The log filename, with path
int fileHandle = -1;				// This is the filehandle that we are using to write.
int echoHandle = -1;				// This is descriptor to echo the content with
int errored = 0;					// Is the log permanently errored?

// Functional prototypes
int openLog( void );
int closeLog( void );

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : enableLogLevels
// Description  : Turn on different log levels
//
// Inputs       : lvl - the log level to enable
// Outputs      : none

void enableLogLevels( unsigned long lvl ) {
	// Set and return, no return code
	logLevel |= lvl;
	return;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : disableLogLevels
// Description  : Turn off different log levels
//
// Inputs       : lvl - the log level to disable
// Outputs      : none

void disableLogLevels( unsigned long lvl ) {
	// Set and return, no return code
	logLevel &= (lvl^0xffffffff);
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : levelEnabled
// Description  : Are any of the log levels turned on?
//
// Inputs       : lvl - the log level(s) to check
// Outputs      : !0 if any levels on, 0 otherwise

int levelEnabled( unsigned long lvl ) {
	return( (logLevel & lvl) != 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : setEchoDescriptor
// Description  : Set a file handle to echo content to
//
// Inputs       : eh - the new file handle
// Outputs      : !0 if any levels on, 0 otherwise

//
void setEchoDescriptor( int eh ) {
	echoHandle = eh;
	return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : initializeLogWithFilename
// Description  : Create a log with a given filename
//
// Inputs       : logname - the log filename
// Outputs      : none

int initializeLogWithFilename( const char *logname ) {

	// Setup library global variables
    logLevel = DEFAULT_LOG_LEVEL;
    fileHandle = -1;
    echoHandle = -1;
    errored = 0;
    memset( descriptors, 0x0, sizeof(descriptors) );
    logFilename = (logname == NULL) ? LOG_SERVICE_NAME : logname;

    // Register the default levels
	registerLogLevel( LOG_ERROR_LEVEL_DESC, 1 );
	registerLogLevel( LOG_WARNING_LEVEL_DESC, 1 );
	registerLogLevel( LOG_INFO_LEVEL_DESC, 0 );
	registerLogLevel( LOG_OUTPUT_LEVEL_DESC, 1 );

    // Return successfully
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : initializeLogWithFilehandle
// Description  : Create a log with a fixed file handle
//
// Inputs       : out - the file handle to use
// Outputs      : 0 if successfully, -1 if failure

int initializeLogWithFilehandle( int out ) {

	// Setup library global variables
    logLevel = DEFAULT_LOG_LEVEL;
    fileHandle = out;
    echoHandle = -1;
    errored = 0;
    memset( descriptors, 0x0, sizeof(descriptors) );
    logFilename = LOG_SERVICE_NAME;

    // Register the default levels
	registerLogLevel( LOG_ERROR_LEVEL_DESC, 1 );
	registerLogLevel( LOG_WARNING_LEVEL_DESC, 1 );
	registerLogLevel( LOG_INFO_LEVEL_DESC, 0 );
	registerLogLevel( LOG_OUTPUT_LEVEL_DESC, 1 );

    // Return successfully
    return( 0 );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : registerLogLevel
// Description  : Register a new log level
//
// Inputs       : descriptor - a description of the new level
//                enable - flag indicating whether the level should be enabled
// Outputs      : the new log level, -1 if none available

unsigned long registerLogLevel( const char *descriptor, int enable ) {

	// Local variables
	int i, lvl;

	// Look for an open descriptor table
	for ( i=0; i<MAX_LOG_LEVEL; i++ ) {
		if ( descriptors[i] == NULL ) {

			// Compute level, enable (if necessary), save descriptor and return
			lvl = 1<<i;
			if ( enable ) enableLogLevels(lvl);
			descriptors[i] = strdup(descriptor);
            return( lvl );
		}
	}

    // Return no room left to create a new log level
	fprintf( stderr, "Too many log levels [%s]", logFilename );
	return( -1 );
}

//
// Logging functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : logMessage
// Description  : Log a "printf"-style message
//
// Inputs       : lvl - the levels to log on, format (etc)
// Outputs      : 0 if successful, -1 if failure

int logMessage( unsigned long lvl, const char *fmt, ...) {

    // Call the the va list version of the logging message
	va_list args;
	va_start(args, fmt);
	int ret = vlogMessage( lvl, fmt, args );
    va_end(args);

    // Return the log return
    return( ret );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : sLog::vlogMessage
// Description  : Log a "printf"-style message (va list prepared)
//
// Inputs       : lvl - the levels to disable,
//                fmt - format (etc)
//                args - the list of arguments for log message
// Outputs      : 0 if successful, -1 if failure

int vlogMessage( unsigned long lvl, const char *fmt, va_list args ) {

	// Local variables
    char msg[MAX_LOG_MESSAGE_SIZE], tbuf[MAX_LOG_MESSAGE_SIZE];
    int first = 1, ret, writelen, i;
    time_t tm;

	// Bail out if not read, open file if necessary
    if ( !levelEnabled(lvl) ) {
    	return( 0 );
    }

    // Check to see if we have a valid filehandle for the log
    if ( fileHandle == -1 ) {
    	openLog();
    }

    // Check for error state before moving forward
    if ( errored ) {
    	return( errored );
    }

    // Add header with descriptor names
    time(&tm);
    strncpy(tbuf, ctime((const time_t *)&tm), MAX_LOG_MESSAGE_SIZE);
    tbuf[strlen(tbuf)-1] = 0x0;
    strncat(tbuf, " [", MAX_LOG_MESSAGE_SIZE);
    for ( i=0; i<MAX_LOG_LEVEL; i++ ) {
        if ( levelEnabled((1<<i)&lvl) ) {

        	// Comma separate the levels if necessary
            if ( !first ) {
            	strncat(tbuf, ",", MAX_LOG_MESSAGE_SIZE);
            } else {
            	first = 0;
            }

            // Add the level descriptor
            if ( descriptors[i] == NULL ) {
            	strncat(tbuf, "*BAD LEVEL*", MAX_LOG_MESSAGE_SIZE);
            } else {
                strncat(tbuf, descriptors[i], MAX_LOG_MESSAGE_SIZE);
            }
          }
    }
    strncat(tbuf, "] ", MAX_LOG_MESSAGE_SIZE);

    // Setup the "printf" like message, get write len
	vsnprintf( msg, MAX_LOG_MESSAGE_SIZE, fmt, args );
    strncat(tbuf, msg, MAX_LOG_MESSAGE_SIZE);
    writelen = strlen(tbuf);

    // Check if we need to CR/LF the line
    if ( tbuf[writelen-1] != '\n' ) {
    	strncat(tbuf, "\n", MAX_LOG_MESSAGE_SIZE);
        writelen = strlen(tbuf);
    }

    // Echo, then Write the entry to the log and return
    if (echoHandle != -1 ) {
    	ret = write( echoHandle, tbuf, writelen );
    }
    if ( (ret=write(fileHandle, tbuf, strlen(tbuf))) != writelen ) {
    	fprintf( stderr, "Error writing to log : %s [%s] (%d)", tbuf, logFilename, ret );
    }
    return( ret );
}

//
// Private Interfaces

////////////////////////////////////////////////////////////////////////////////
//
// Function     : openLog
// Description  : Open the log for processing
//
// Inputs       : none
// Outputs      : 0 if successful, -1 otherwise

int openLog( void ) {

	// If no filename, then use STDERR
	if ( logFilename == NULL ) {
		fileHandle = 2;
	} else {

		// Open the log for writing (append if existing)
        if ( (fileHandle = open(logFilename, O_APPEND|O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR)) == -1 ) {
        	// Error out
		    errored = 1;
		    fprintf( stderr, "Error opening log : %s (%s)",	logFilename, strerror(errno) );
        }
	}

    // Return successfully
    return( errored );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : closeLog
// Description  : Close the log
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int closeLog( void ) {

	// Local variables
	int i;

	// Cleanup the structures
	for ( i=0; i<MAX_LOG_LEVEL; i++ ) {
		if ( descriptors[i] != NULL )
			free( descriptors[i] );
	}

	// Close and return successfully
	close( fileHandle );
	fileHandle = -1;
    return( 0 );
}


