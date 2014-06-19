////////////////////////////////////////////////////////////////////////////////
//
//  File          : smsa_srvr.c
//  Description   : This is the server program for the SMSA system.
//
//  Author        : Patrick McDaniel
//  Last Modified : Sun Nov  3 18:17:41 PST 2013
//

// Include Files
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

// Project Includes
#include <smsa.h>
#include <smsa_network.h>
#include <cmpsc311_log.h>

// Defines
#define SMSA_ARGUMENTS "vhl:"
#define USAGE \
	"USAGE: smsasrvr [-h] [-v] [-l <logfile>]\n" \
	"\n" \
	"where:\n" \
	"    -h - help mode (display this message)\n" \
	"    -v - verbose output\n" \
	"    -l - write log messages to the filename <logfile>\n" \
	"\n" \

//
// Global Data

//
// Functional Prototypes

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : main
// Description  : The main function for the SMSA simulator
//
// Inputs       : argc - the number of command line parameters
//                argv - the parameters
// Outputs      : 0 if successful, -1 if failure

int main( int argc, char *argv[] )
{
	// Local variables
	int ch, verbose = 0, log_initialized = 0;
	int port;

	port = atoi(argv[1]);
	// Process the command line parameters
	while ((ch = getopt(argc, argv, SMSA_ARGUMENTS)) != -1) {

		switch (ch) {
		case 'h': // Help, print usage
			fprintf( stderr, USAGE );
			return( -1 );

		case 'v': // Verbose Flag
			verbose = 1;
			break;

		case 'l': // Set the log filename
			initializeLogWithFilename( optarg );
			log_initialized = 1;
			break;

		default:  // Default (unknown)
			fprintf( stderr, "Unknown command line option (%c), aborting.\n", ch );
			return( -1 );
		}
	}

	// Setup the log as needed
	if ( ! log_initialized ) {
		initializeLogWithFilehandle( CMPSC311_LOG_STDERR );
	}
	if ( verbose ) {
		enableLogLevels( LOG_INFO_LEVEL );
	}

	printf ( "port = %d", port );

	// Run the server
	smsa_server( port );

	// Return successfully
	return( 0 );
}

