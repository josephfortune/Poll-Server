#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <poll.h>
#include "network.h"
#include "logger.h"

#define MAX_CONNECTIONS 	1024

struct pollfd socketList[MAX_CONNECTIONS];

int createTCPSocket(int port) 
{	
	int status; 				// In case of error during getaddrinfo
	int sockfd;
	struct addrinfo hints;
	struct addrinfo *servinfo; 		// points to linked list of results
	int yes = 1;

	memset(&hints, 0, sizeof(hints)); 	// Zero-out the struct
	hints.ai_family 	= AF_UNSPEC; 	// don't care IP4 or IP6
	hints.ai_socktype 	= SOCK_STREAM; 	// TCP
	hints.ai_flags 		= AI_PASSIVE; 	// fill in my IP for me

	// Convert port number to string (Thats how getaddrinfo wants it)
	char port_str[6];
	sprintf(port_str, "%d", port);

	// Get Address Info
	if ((status = getaddrinfo(NULL, port_str, &hints, &servinfo)) != 0) 
	{
		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		exit(1);
	}

	// Create socket
	if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1)
	{
		perror("Error creating socket");
		exit(1);
	}

	// Set socket option
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) 
	{
		perror("Error setting socket option");
		exit(1);
	} 

	// Set non-blocking
	if (ioctl(sockfd, FIONBIO, (char *)&yes) == -1)
	{
		perror("Error using ioctl to set socket to non-blocking");
		exit(1);
	}
  
	// Bind socket
	if (bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
	{
		perror("Failed to bind socket");
		exit(1);
	}

	// Listen
	if (listen(sockfd, BACKLOG) == -1)
	{
		perror("Error while attempting to listen");
		exit(1);
	}

	freeaddrinfo(servinfo);
	return sockfd;
}



int sendString(int sockfd, char *fmt, ...) 
{
	int sent_bytes, bytes_to_send;
	char msg_buff[MAX_MSG_SIZE] = "NULL";

	// Handle format string
	va_list         list;
	char            *p, *r, *d;
	int             e;

	va_start(list, fmt);
	for (p = fmt, d = msg_buff; *p ; ++p)
	{
		int length;
		if ( *p != '%' ) // Simple string
		{
			*d = *p;
			d++;
		}          
		else
		{
			switch ( *++p )
			{
				// string
				case 's':
				{
					r = va_arg( list, char * );
					length = strnlen(r, MAX_MSG_SIZE);
					strncpy(d, r, length);
					d += length;
					continue;
				}

				// int
				case 'd':
				{
					e = va_arg( list, int );
					d += sprintf(d,"%d", e);
					continue;
				}

				default:
				{
					d = p;
					d++;
				}
			}
		}
	}
	va_end( list );

	// Send the message buffer over the socket
	char *buff = msg_buff;
	bytes_to_send = strlen(msg_buff);
	while(bytes_to_send > 0) 
	{
		sent_bytes = send(sockfd, buff, bytes_to_send, 0);
		if(sent_bytes == -1)
			return 0; // return 0 on send error
		bytes_to_send -= sent_bytes;
		buff += sent_bytes;
	}
	return 1; // return 1 on success
}



int recvLine(int sockfd, char *dest_buffer) {
#define EOL "\r\n" // End-Of-Line byte sequence
#define EOL_SIZE 2
	char *ptr;
	int eol_matched = 0;
	int len = 0; // To make sure the msg doesn't overflow the buffer   

	ptr = dest_buffer;
	while(recv(sockfd, ptr, 1, 0) == 1 && len < MAX_MSG_SIZE) // read a single byte
	{ 
		// Clean byte of any non-ascii characters
		if (*ptr < 0 || *ptr > 127)
			*ptr = '#'; 

		if(*ptr == EOL[eol_matched]) // does this byte match terminator
		{ 
			eol_matched++;
			if(eol_matched == EOL_SIZE) // if all bytes match terminator,
			{ 
				*(ptr+1-EOL_SIZE) = '\0'; // terminate the string
				return strnlen(dest_buffer, MAX_MSG_SIZE); // return bytes recevied
			}
		} 
		else 
		{
         		eol_matched = 0;
      		}   
		ptr++; // increment the pointer to the next byte;
		len++; // Make sure we don't overflow buffer
	}
	*ptr = '\0'; // NULL-terminate line (hopefully this fixes the crashing bug I fear is caused by strstr reading non-terminated strings)
	return len; // didn't find the end of line characters
}



void startServer(int port)
{
	// Socket stuff
	struct sockaddr_storage theirAddress;
	socklen_t addressSize;
	int listenSocket, newSocket, i, pollResult, socketListSize;	
	
	socketListSize = 0;
	addressSize = sizeof(theirAddress);

	// Initialize the socket list
	memset(socketList, 0, sizeof(struct pollfd) * MAX_CONNECTIONS);

	// Initialize the listen socket
	listenSocket = createTCPSocket(port);

	// Add the listen socket as first entry
	socketList[0].fd 	= listenSocket;
	socketList[0].events 	= POLLIN;
	socketListSize++;	
	plog("Server started. Listening on port %d", port);


	// Poll the sockets listening for incoming messages
	while(1)
	{
		pollResult = poll(socketList, socketListSize, 2000);

		// If error
		if (pollResult == -1)
		{
			perror("Error Polling Sockets");
			exit(1);
		}

		// If timed out (No socket activity)
		if (pollResult == 0)
			continue;

		// Check the listener socket for activity
		if (socketList[0].revents != 0)
		{
			if (socketList[0].revents == POLLIN)
			{
				// There is a new connection
				newSocket = accept(listenSocket, (struct sockaddr *)&theirAddress, &addressSize);
				if (newSocket == -1)
				{
					perror("Error accepting new connection");
					exit(1);
				}
				
				// Add the new connecting socket to list
				socketList[socketListSize].fd		= newSocket;
				socketList[socketListSize].events	= POLLIN;
				socketListSize++;
				
			}
			else
			{
				printf("Unexpected error from listener socket during polling. revents == %d\n", socketList[0].revents);
				exit(1);
			}
		}

		// Check the other sockets for activity
		for (i = 1; i < socketListSize; i++)
		{
			if (socketList[i].revents != 0)
			{
				char tempBuff[MAX_MSG_SIZE+1];
				plog("Found a talking socket at position: %d", i);
				recvLine(socketList[i].fd, tempBuff);
				plog("Received: %s", tempBuff);
				sendString(socketList[i].fd, "Hello there!");
			}
		}

		// Here the socketList needs to be defragged so that all active sockets are adjacent and at the beginning of the list ---------------------------------
	}
}





