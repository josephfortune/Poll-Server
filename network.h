#define BACKLOG		10
#define MAX_MSG_SIZE	1024

int createTCPSocket(int port);
int sendString(int sockfd, char *fmt, ...);
int recvLine(int sockfd, char *dest_buffer);
void startServer(int port);
