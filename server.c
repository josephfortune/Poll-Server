#include <stdio.h>
#include <stdlib.h>
#include "network.h"

int main(int argc, char* argv[]) 
{ 
	int port;

	// Validate command line args
	if (argc != 2)
	{
		printf("Usage: %s <port>\n", argv[0]);
		return 0;
	}
  
	// Validate port argument
	if (!(port = atoi(argv[1])))
	{
		printf("Invalid port number: %s\n", argv[1]);
		return 0;
	}

	startServer(port);
}













