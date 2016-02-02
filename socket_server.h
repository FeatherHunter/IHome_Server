#ifndef _H_SOCKET
#define _H_SOCKET


#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define SERVER_PORT 8080
#define SERVER_ADDR "139.129.19.115"
int init_server(int domain, int port, char * straddr);

#endif // _H_SOCKET
