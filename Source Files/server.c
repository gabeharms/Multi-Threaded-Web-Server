////////////////////////////////////////////////////////////////////////////////
//
//  File          : server.c
//  Description   : This server uses the sockets API in C to receive HTTP requests and serve responses
//					back. It calls methods from the server_thread files in order to serve multiple clients
//					simultaneously 
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
#include <smsa_threads.h>

/* DEBUG */
#define DEBUG 1
#define MAXLINE 1000
#define MAXBUF 100000
#define MAX_NUM_OF_HEADER_LINES 10
#define MAX_THREADS 5


// Global Variables
int serverShutdown;
MY_THREAD backlog[MAX_THREADS];


//Functional Prototypes
int setupServer ( int *server, int port );
void * processClient ( void * args ); 
int read_request_hdrs ( int client );
int parse_uri ( char *uri, char *filename, char *cgiargs );
int serve_static ( int client,  char *filename, int filesize );
void get_filetype ( char *filename, char *filetype );
int serve_dynamic ( int client, char *filename, char *cgiargs );
int readBytes ( int server, int len, char *block );
int sendBytes ( int server, int len, char *block );
int selectData ( int sock, int wait );
void signalHandler ( int signal );


////////////////////////////////////////////////////////////////////////////////
//
// Function     : server
// Description  : The main function server processing loop.
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int server ( int port ) {


	
	struct sockaddr_in clientAddress;  //holds client address
	int server;			   //file handle for the socket
	int client;			   //file handle for the client
	unsigned int inet_len;		
	

	//Set up the server to be listening
	setupThreads ( backlog, MAX_THREADS );

	if ( setupServer ( &server, port ) ) {
		logMessage ( LOG_ERROR_LEVEL, "_smsa_server:Failed to properly set up the server" );
		return 1;
	}
	
	//Loop until server needs to shutdown
	serverShutdown = 0;
	while ( !serverShutdown ) {
	
		logMessage ( LOG_INFO_LEVEL, "Now Waiting for Data to Come In..");


		//The select data function will use the select API to wait for data from
		//the new connection to come in. Once it detects that there is available data
		//to read, the process will continue
		if ( selectData ( server, 1 ) ) {
			logMessage ( LOG_ERROR_LEVEL, "_smsa_server:Failed to select data [%s]", strerror(errno) );
			return 1;
		}
		if ( areThreadsMaxedOut( MAX_THREADS) ) {
		
			//Accept the connection to the NEW requesting client
			inet_len = sizeof( clientAddress );
			if ( (client = accept ( server, (struct sockaddr*)&clientAddress, &inet_len)) == -1 ) {
				logMessage( LOG_ERROR_LEVEL, "_smsa_server:Failed to accept connection [%s]", strerror(errno) );
				return 1;
			}

			logMessage ( LOG_INFO_LEVEL, "New Client Connection Recieved [%s/%d]", inet_ntoa(clientAddress.sin_addr), clientAddress.sin_port ); 

			pthread_create ( &backlog[findFreeThread(backlog, MAX_THREADS)].thread, NULL, processClient, (void *)&client );
			
		}
		else {
			waitForThreads ( backlog, MAX_THREADS );
		
		}
	}

	//Shutting down the server
	logMessage ( LOG_INFO_LEVEL, "Shutting Down the Server..." );
	close ( server );
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : processClient
// Description  : Handles all client requests after a new connection comes in
//
// Inputs       : server - socket file handle
// Outputs      : 0 if successful, -1 if failure
void * processClient ( void * arg ) {
	int ret = 0;
	int is_static;				//Boolean variable to hold the return of the parse_uri function
	struct stat sbuf;			//Helps determine size of file with stat function
	char buf[MAXLINE];			//Holds the result of the readBytes function, which reads from the client
	char method[MAXLINE];			//Part of the buf variable that holds the type of request ( GET )
	char uri[MAXLINE];			//Part of the buf variable that holds the requested file's PATH	with arguements
	char version[MAXLINE];			//Part of the buf which specifies the type of request ( HTTP/1.0 )
	char filename[MAXLINE];			//Name of the requested file without the arguements
	char cgiargs[MAXLINE];			//Arguements extracted from the uri ( cgiargs = uri - filename )

	int *client = (int *)arg;

	//The select data function will use the select API to wait for data from
	//the new connection to come in. Once it detects that there is available data
	//to read, the process will continue
	if ( selectData ( *client, 1 ) ) {
		logMessage ( LOG_ERROR_LEVEL, "_smsa_server:Failed to select data [%s]", strerror(errno) );
		ret = 1;
		pthread_exit( &ret);
	}
	
	//Now that data has been selected to read, we will read it.
	//This part of the read will be the initial request header from
	//the client of the format: method uri version. 
        if ( (readBytes( *client, MAXLINE, buf ) == -1 )) {
		logMessage ( LOG_INFO_LEVEL, "No data was read from new client... Closing connection" );
		ret = 1;
		pthread_exit(&ret);
	}

	logMessage ( LOG_INFO_LEVEL, "Client Request is = %s", buf );

        //Now that we have the initial request fromt the client, we will parse 
	//it into three different, USABLE strings
        sscanf ( buf, "%s %s %s", method, uri, version );

        //Confirm that it is a "GET" request. If it is not, deny the
	//the client request
        if ( strcasecmp( method, ( "GET" ) ) ) {
                logMessage ( LOG_INFO_LEVEL, "We do not implement the %s function. 501 error", method );
                ret = 1;
		pthread_exit( &ret );
        }

	//Now we will read the headers of the request, if it is of the
	//HTTP/1.1. If it is not, there are no headers, so we will not 
	//attempt to read them. Right now, we do nothing with the headers
	//but this function provides future capabilites dealing with these
	//headers
        if ( !strcmp ( version, "HTTP/1.1" ) )
                read_request_hdrs ( *client );

        //Call the parse_uri function to extract the filename and
	//arguements from the uri we recieved in the request. This 
	//will allow us to figure what file to open, and if there are
	//any arguements with it. The return of the parse_uri function
	//tells us whether it is static or dynamic data that has been 
	//requested
        is_static = parse_uri ( uri, filename, cgiargs );

	//Use the stat function to find the needed information of the file 
	//that was requested. This will give us the permissions as well as,
	//more importantly, the size of the file. Also if the stat function 
	//returns less than zero, we know that the file doesn't exist.
        if ( stat(filename, &sbuf) < 0 ) {
                logMessage ( LOG_ERROR_LEVEL, "the %s file could not be found", filename );
		ret = 1;
                pthread_exit(&ret);
        }

        //Here we will use macros, and the result of the stat function stored in sbuf, to
	//determine if the file is a regular file. Meaning that the file is not a directory,
	//or anything like that
        if ( is_static ) {	//Static Content
                if ( !(S_ISREG( sbuf.st_mode )) || !(S_IRUSR & sbuf.st_mode ) ) {
                        logMessage ( LOG_ERROR_LEVEL, "Can't read the file. 403 ERROR");
                        ret = 1;
			pthread_exit(&ret);
                }
		//Send static data
                serve_static( *client, filename, sbuf.st_size );
        }
        else {		       //Dynamic Content

                if ( !(S_ISREG( sbuf.st_mode )) || !(S_IXUSR & sbuf.st_mode ) ) {
                        logMessage ( LOG_ERROR_LEVEL, "Can't read the file. 403 ERROR");
                        ret = 1;
			pthread_exit( &ret );
                }
		//Send dynamic data
                serve_dynamic( *client, filename, cgiargs );
        }

	buf[0] = '\0';
			
	//Done with the request, now close it
	logMessage( LOG_INFO_LEVEL, "Closing client connection" );
        close( *client );

	pthread_exit (&ret);

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : read_request_hdrs
// Description  : read the hdrs from the request
//
// Inputs       : client - socket file handle
// Outputs      : 0 if successful, -1 if failure
int read_request_hdrs ( int client ) {

	char buf[MAXLINE];
	int count = 0;

	while ( (readBytes ( client, MAXLINE, buf )) != -2 && count < MAX_NUM_OF_HEADER_LINES) {
		logMessage ( LOG_INFO_LEVEL, "%s", buf );
		count++;
	}
	
	buf[0] = '\0';
	logMessage ( LOG_INFO_LEVEL, "%s", buf );
	return 0;

}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : parse_uri
// Description  : read the uri, and find the filename and the cgiargs
//
// Inputs       : uri - string to decipher
//		  filename - string to place the filename in
//		  cgiargs - string to hold the arguements
// Outputs      : 0 if dynamic , 1 if static
int parse_uri ( char *uri, char *filename, char *cgiargs ) {

	char *ptr;
	
	//Check if the content is static or dynamic
	if ( !strstr ( uri, "cgi-bin" ) ) {	//Static Content
		strcpy ( cgiargs, "" );				//No Arguements
		strcpy ( filename, ".." );			//Adds the dot to denote current directory
		strcat ( filename, uri );			//Places the "/blah.blah" on top of the dot
		if ( uri[strlen(uri)-1] == '/' )
			strcat ( filename, "pages/index.html");	//if the uri ends in a slash, add home.html
		logMessage ( LOG_INFO_LEVEL, "Filename = %s", filename );
		return 1;					//return as static
	}
	else {				       //Dynamic Content
		ptr = index( uri, '?');				//find and parse the arguements	
		if ( ptr ) {
			strcpy ( cgiargs, ptr+1 );
			*ptr = '\0';
		}
		else 
			strcpy ( cgiargs, "");
		
		strcpy ( filename, "" );
		strcat ( filename, uri );
		return 0;					//return as dynamic
	}
	
	ptr = NULL;
				
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : serve_static
// Description  : send the static response to the client
//
// Inputs       : client - socket file handle
//		  filename - name of the file to read
//		  filesize - size of the file
// Outputs      : 0 if successful, -1 if failure
int serve_static ( int client, char *filename, int filesize ) {

	int srcfd;			//file descriptor for our requested file
	char *srcp;			//pointer to the memory mapped data we dynamically allocate
	char  filetype[MAXLINE];	//String containting the type of file, so send to the client
	char  buf[MAXBUF];		//variable to hold the compiled data to be sent
	
	//Send response headers to client
	get_filetype ( filename, filetype);
	sprintf (buf, "HTTP/1.0 200 OK\r\n" );
	sprintf (buf, "%sServer: Gabe Harms Web Server\r\n", buf );
	sprintf (buf, "%sContent-length: %d\r\n", buf, filesize );
	sprintf (buf, "%sContent-type: %s\r\n\r\n", buf, filetype );		//the extra \r\n is explicit and neccessary
	sendBytes ( client, strlen(buf), buf);					//Send the header to the client

	logMessage ( LOG_INFO_LEVEL, "Header sent to browser" );

	//Send response body to client
	srcfd = open( filename, O_RDONLY, 0 );					//open the file in read-only format
	srcp = mmap ( 0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0 );		//memory map the file, and have srcp point to it
	close ( srcfd );							//close the file
	sendBytes ( client, filesize, srcp );					//Send the file content to the client
	munmap ( srcp, filesize );						//Unallocate the memory for the file

	//Reset values
	buf[0] = '\0';
	filetype[0] = '\0';
	srcp = NULL;
	return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_filetype
// Description  : find the type of the file that we are going to read
//
// Inputs       : filename - name of the file to read
//		  filetype - type of the file that we are going to read
// Outputs      : 0 if successful, -1 if failure
void get_filetype ( char *filename, char *filetype ) {
	
	if ( strstr ( filename, ".html" ) )
		strcpy ( filetype, "text/html" );
	else if ( strstr ( filename, ".gif" ) )
		strcpy ( filetype, "image/gif" );
	else if ( strstr ( filename, ".jpg" ) )
		strcpy ( filetype, "image/jpeg" );
	else 
		strcpy ( filetype, "text/plain" );
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : serve_dynamic
// Description  : send the dynamic response to the client
//
// Inputs       : client - socket file handle
//                filename - name of the file to read
//                filesize - size of the file
// Outputs      : 0 if successful, -1 if failure
int serve_dynamic ( int client, char *filename, char *cgiargs ) {

	char buf[MAXLINE];
	//char *emptylist[] = { NULL };

	//Return first part of HTTP response
	sprintf ( buf, "HTTP/1.0 200 OK\r\n" );
	sendBytes ( client, strlen(buf), buf );
	sprintf( buf, "Server: Gabe Harms Web Server" );
	sendBytes ( client, strlen(buf), buf );
	
	if ( fork() == 0 ) { //Child
		//A real server would set all the CGI vars here
		setenv ( "QUERY_STRING", cgiargs, 1);
		dup2( client, STDOUT_FILENO);		//redirect stdout to client
		//Here is where you put your dynmaic function and call it like so
		//execve ( filename, emptylist, environ );	
	}
	wait ( NULL );

	return 0;
}





////////////////////////////////////////////////////////////////////////////////
//
// Function     : readBytes
// Description  : Read a certain amount of bytes from the client
//
// Inputs       : server - socket file handle
//		  len - amount of bytes to read
//		  block - the read of the block
// Outputs      : 0 if successful, -1 if failure

int readBytes ( int server, int len, char *block ) {
	
	int rb, i;
	char temp[len];

	//Run until all bytes have been read which is when the amount
	//of bytes that have been read ( readBytes ) is no longer less then
	//the amount of bytes total that need to be read ( len ).
	for ( i = 0; i < len; i++ ) {

		//Read byte into "block", at the index of readBytes, from the socket ( server ). 
		//The readBytes allows us to move across the "block" so that we don't read over stuff
		//on the "block". rb will contain the amount of bytes that were able to be read. We wish for
		//this to be the entire len variable, but it might not be ready for us to read the first time.
		//This loop will allow us to continue reading until we have read the amount of bytes in len.
		if ( ( rb = read( server, &temp[i], 1 ) ) < 0 ) { //Reading Error
			logMessage( LOG_ERROR_LEVEL, "_readBytes:Failed to read a byte [%s]", strerror(errno) );
			return 1;
		}
		else if ( rb == 0 && i == 0 ) {			//No data read 
			logMessage ( LOG_INFO_LEVEL, "No Data read" );
			return -1;
		}
		if ( temp[i] == '\n' ) {			//Found line break ( No Action required )
			if ( temp[i-1] == '\r' && i == 1) {  	//Found end of a request line ( Return, but copy first )
				strcpy ( block, temp );		
				block[i+1] = '\0';
				return -2;
			}
			break;
		}
	}

	if ( DEBUG )
		logMessage ( LOG_INFO_LEVEL, "Successfully Read [%d] Bytes", i );
	
	strcpy ( block, temp);

	//reset temp
	block[i+1] = '\0';
	return 0;

}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : sendBytes
// Description  : Read a certain amount of bytes from the client
//
// Inputs       : server - socket file handle
//		  len - amount of bytes to read
	//		  block - the read of the block
// Outputs      : 0 if successful, -1 if failure

int sendBytes ( int server, int len, char *buf ) {

	// Local variables
    int sentBytes = 0, sb;

    // Loop until you have read all the bytes
	while ( sentBytes < len ) {

		// Read the bytes and check for error
		if ( (sb = write(server, &buf[sentBytes], len-sentBytes)) < 0 ) {
	    		logMessage( LOG_ERROR_LEVEL, "SMSA send bytes failed : [%s]", strerror(errno) );
	    		return( -1 );
			}

			// Check for closed file
			else if ( sb == 0 ) {
	    		// Close file, not an error
	    		logMessage( LOG_ERROR_LEVEL, "SMSA client socket closed on snd : [%s]", strerror(errno) );
	    		return( -1 );
			}

			// Now process what we read
			sentBytes += sb;
	    	}


	if ( DEBUG )
		logMessage ( LOG_INFO_LEVEL, "Successfully Sent [%d] Bytes", sentBytes );

    // Return successsfully
    return( 0 );

}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : selectData
// Description  : Waits for data and then selects of it to be processed
//
// Inputs       : int - server file handler
// Outputs      : 0 if successful, -1 if failure

int selectData ( int sock, int wait ) {

	fd_set readEvent;
	int maxSocketChecks = sock + 1;

	//Initialize and set the file descriptors
	FD_ZERO( &readEvent );
	FD_SET( sock, &readEvent );

	struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;

        //select and wait
        if ( select( maxSocketChecks, &readEvent, NULL, NULL, (wait) ? NULL: &tv ) == -1 ) {
                logMessage( LOG_INFO_LEVEL, "No data left to read" );
                return 1;
        }


	//make sure we are selected on the read
	if ( FD_ISSET ( sock, &readEvent ) == 0 ) {
		logMessage( LOG_ERROR_LEVEL, "No data left to read");
		return 1;
	}

		
	logMessage ( LOG_INFO_LEVEL, "Selected Data. Connecting to the Client..." );
	return 0;

}



////////////////////////////////////////////////////////////////////////////////
//
// Function     : setUpServer
// Description  : Sets up the server by creating the socket, binding it to the right addresss
//		  and starts the server listening. It then sets the server input equal to the
//		  socket file handler.
//
// Inputs       : int - server file handler
// Outputs      : 0 if successful, -1 if failure

int setupServer ( int *server, int port ) {


	struct sigaction sigINT;   	   //holds the sigINT signal handler
	struct sockaddr_in serverAddress;  //holds server addres
	int optionValue = 1;		   //holds the value for setsocketopt function call


	//Set signal handler
	//This sets changes the current SIGINT signal handler to operate in the way
	//that we would like by having our smsa_signal_handler be called on reception
	//of a SIGINT from the OS, rather than the default handler
	sigINT.sa_handler = signalHandler;
	sigINT.sa_flags = SA_NODEFER | SA_ONSTACK;
	sigaction ( SIGINT, &sigINT, NULL );


	//Create the socket
	//Set up a socket using TCP protocol ( SOCK_STREAM ), and the address family 
	//version of inet, while setting the server variable to the file handle
	if ( ( *server = socket ( AF_INET, SOCK_STREAM, 0 ) ) == -1 ) {
		logMessage( LOG_ERROR_LEVEL, "_setUpServer:Failed to set up the socket [%s]", strerror(errno) );
		return 1;
	}

	logMessage( LOG_INFO_LEVEL, "Socket Successfully Initialized. Socket File Handle = %d", *server );
	
	//Set the socket to be reusable. 
	//The setsockopt will allow us to change options of our socket which is 
	//specified with our file handle, server.
	if ( setsockopt ( *server, SOL_SOCKET, SO_REUSEADDR, &optionValue, sizeof(optionValue) ) != 0 ) {
		logMessage ( LOG_ERROR_LEVEL, "_setUpServer:setsockopt failed to make the local address reusable [%s]", strerror(errno) );
		return 1;
	}

	if ( DEBUG )
		logMessage ( LOG_INFO_LEVEL, "Socket Set Up To Reuse Addresses" );

	logMessage ( LOG_INFO_LEVEL, "Port = %d", port );

	//Set up server address
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons( (unsigned short )port );
	serverAddress.sin_addr.s_addr = htonl( INADDR_ANY );


	//bind the server to the socket. bind the server file handle to 
	//the address we have initialized
	if ( bind ( *server, (struct sockaddr*)&serverAddress, sizeof( struct sockaddr) ) == -1 ) {
		logMessage ( LOG_ERROR_LEVEL, "_setUpServer:Failure to bind the server to the socket [%s]", strerror(errno) );
		return 1;
	}
	
	if ( DEBUG )
		logMessage ( LOG_INFO_LEVEL, "Socket Is Now Bound To Any Address");

	//listen for connections
	if ( listen ( *server, SMSA_MAX_BACKLOG ) == -1 ) {
		logMessage ( LOG_ERROR_LEVEL, "_setUpServer:Failure to properly listen [%s]", strerror(errno) );
		return 1;
	}
	
	logMessage ( LOG_INFO_LEVEL, "Socket Is Now Listening Queueing %d Connections", SMSA_MAX_BACKLOG );


	logMessage ( LOG_INFO_LEVEL, "Server Has Now Been Successfully Setup on port %d", port );
	return 0;

}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : signalHandler
// Description  : Handles a SIGINT from the operating system and changes the serverShutdown
//		  variable equal to zero.
//
// Inputs       : int - server file handler
// Outputs      : 0 if successful, -1 if failure

void signalHandler ( int signal ) {

	logMessage ( LOG_ERROR_LEVEL, "_signalHandler: Following Signal recieved %d. Shutting down Server", signal );
	serverShutdown = 1;


}

