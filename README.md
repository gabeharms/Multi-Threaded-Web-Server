Web_Server
==========

All files in the Source Files directory were written by Gabe Harms

The server.c file contains the main server loop, and is run continously until the server is turned off. The sockets
API in c is used in order to handle the HTTP requests. Multiple clients can be handled at the same time thanks to the 
server_thread files which initialize new threads each time a request comes in.
