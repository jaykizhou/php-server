#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>

#define PROCESS_NUM 5

static int create_and_bind(int port)
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);

    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);

    bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));

    return fd;
}

static int set_socket_nonblock(int fd) 
{
    int flags, ret;
    if ((flags = fcntl(fd, F_GETFL, 0)) < 0) {
        printf("fcntl error\n");
        return -1；
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        printf("fcntl error\n");
        return -1;
    }

    return 0;
}
