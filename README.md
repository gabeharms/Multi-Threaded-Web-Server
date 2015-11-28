### Web-Server
This project contains a web server built from scratch in C. The server.c file contains the main server loop, and is run continously until the server is turned off. The sockets API in C is used in order to handle the HTTP requests. Multiple clients are be handled similtaneously using the server_thread.c files which initializes new threads each time a request comes in.


All files in the Source Files directory were written by Gabe Harms
