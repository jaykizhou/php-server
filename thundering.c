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
#include <sys/stat.h>
#include <semaphore.h>

#define LOCK_NAME_LEN 50
struct slock {
    sem_t *semp;
    char name[LOCK_NAME_LEN];
};

struct slock *s_alloc(const char *name) {
    struct slock *sp;
    
    if ((sp = malloc(sizeof(struct slock))) == NULL) {
        return NULL;
    }
    
    snprintf(sp->name, sizeof(sp->name), "%s", name);

    if ((sp->semp = sem_open(sp->name, O_CREAT, 0644, 1)) == SEM_FAILED) {
        printf("sem_open error\n");
        free(sp);
        return NULL;
    }

    return sp;
}

#define PROCESS_NUM 10
#define MAX_EVENTS 64

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
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        printf("fcntl error\n");
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {

    int sfd, efd, ret, i;
    struct epoll_event event;
    struct epoll_event *events;
    //struct slock *slockp;
    sem_t *sem;
    
    // 创建并绑定套接字
    sfd = create_and_bind(5555);
    if (sfd == -1) {
        printf("create_and_bind error\n");
        return -1;
    }

    // 设置套接字为阻塞模式
    if (set_socket_nonblock(sfd) == -1) {
        printf("set_socket_nonblock error\n");
        return -1;
    }

    if (listen(sfd, SOMAXCONN) < 0 ) {
        printf("listen error\n");
        return -1;
    }

    efd = epoll_create(MAX_EVENTS);
    if (efd < 0) {
        printf("epoll_create error\n");
        return -1;
    }

    event.data.fd = sfd;
    event.events = EPOLLIN;
    if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event) < 0) {
        printf("epoll_ctl error\n");
        return -1;
    }

    // 创建用来接收有事件发生的events
    events = calloc(MAX_EVENTS, sizeof(event));

    // 创建多个进程
    for (i = 0; i < PROCESS_NUM; i++) {
        int pid = fork();

        // 子进程
        if (pid == 0) {

            // 创建一个锁
            sem = sem_open("/thunder_epoll_lock", O_CREAT, 0644, 1);
            if (sem == SEM_FAILED) {
                printf("sem_open error, process %d\n", getpid());
            }

            // 事件循环
            while (1) {
                // 尝试获取锁
                sem_wait(sem);
                printf("process %d get lock!\n", getpid());
                
                int j, n;
                n = epoll_wait(efd, events, MAX_EVENTS, -1);
                printf("process %d return from epoll_wait!\n", getpid());

                //sleep(2);
                for (j = 0; j < n; j++) {
                    if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || 
                            !(events[i].events & EPOLLIN)) {
                        printf("epoll error\n");
                        close(events[i].data.fd);
                        continue;
                    }
                    else if (events[i].data.fd == sfd) {
                        struct sockaddr in_addr;
                        socklen_t in_len;
                        int infd;

                        infd = accept(sfd, &in_addr, &in_len);
                        if (infd == -1) {
                            printf("process %d accept failed!\n", getpid());
                            break;
                        }
                        printf("process %d accept successed!\n", getpid());

                        close(infd);

                        // 释放锁
                        sem_post(sem);
                        //sleep(1);
                    }
                }
            }
        }
    }

    int status;
    wait(&status);
    free(events);
    close(sfd);
    return 0;
}




