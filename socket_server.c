#include "socket_server.h"

/*
    description: have some function about socket
 */

int init_server(int domain, int port, char * straddr)
{
    int socket_fd;
     /*IPV4 TCP select default protocol*/
     struct sockaddr_in addr;
     addr.sin_family = domain;
     addr.sin_port = htons(port);
     addr.sin_addr.s_addr = inet_addr(straddr);
     //addr.sin_addr.s_addr = INADDR_ANY;

    if( (socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        fprintf(stderr, "socket error!%s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if( bind( socket_fd, (struct sockaddr *)&addr, sizeof(addr) ) == -1)
    {
        fprintf(stderr, "bind error!%s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if(listen(socket_fd, SOMAXCONN) == -1 )
    {
        fprintf(stderr, "listen error!%s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    return socket_fd;
}
