#include "mylock.h"
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
#include <signal.h>

#define PROCESS_NUM 4
#define MAX_EVENTS 64

slock *sl;

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

// 信号处理函数
void sig_handler(int sig)
{
    lock_close(sl);
}

// 设置信号的处理函数
void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sigaction(sig, &sa, NULL);
}

// 子进程执行体
int process_run(int sfd, slock *sl) 
{
    int efd, ret, i;
    int ln = 0; // 连接数
    int ls = -1; // 锁函数的返回值
    char buf[100];

    struct epoll_event event;
    struct epoll_event events[MAX_EVENTS];

    // 每个子进程创建自己的epoll fd
    efd = epoll_create(MAX_EVENTS);
    if (efd < 0) {
        printf("epoll_create error\n");
        return -1;
    }

    printf("process %d forked\n", getpid());

    // 事件循环
    while (1) {
        /*
         * 判断连接数是否为0，为0,阻塞等待
         * 不为0,非阻塞等待锁
         */
        printf("process %d have %d connections\n", getpid(), ln);

        // 判断是否已经获得了锁
        if (ls == -1) { // 未获得锁
            if (ln > 0) {
                ls = p(sl, 0);
                printf("process %d get trywait lock\n", getpid());
            } else {
                // 阻塞等待锁
                ls = p(sl, 1);
                if (ls == -1) {
                    continue;
                }
                printf("process %d get wait lock\n", getpid());
            }

            if (ls == 0) {
                // 获取到锁，将监听套接字sfd加入epoll中
                event.data.fd = sfd;
                event.events = EPOLLIN;
                if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event) < 0) {
                    printf("epoll_ctl error\n");
                    printf("\n%s\n", strerror(errno));
                    v(sl);
                    ls = -1;
                    if (ln == 0) {
                        continue;
                    }
                }
                printf("process %d epoll add sfd : %d\n", getpid(), sfd);
            }
        }

        int j, n;
        n = epoll_wait(efd, events, MAX_EVENTS, -1);
        if (n < 1) {
            printf("epoll wait error\n");
            return -1;
        }
        printf("process %d return from epoll_wait %d!\n", getpid(), n);

        //sleep(1);
        for (j = 0; j < n; j++) {

            if ((events[j].events & EPOLLERR) || (events[j].events & EPOLLHUP) || 
                    !(events[j].events & EPOLLIN)) {
                printf("process %d epoll error\n", getpid());
                printf("\n%d : %s\n", errno, strerror(errno));

                // 从epoll删除该监听套接字
                epoll_ctl(efd, EPOLL_CTL_DEL, events[j].data.fd, NULL);

                // 关闭该套接字
                if (events[j].data.fd != sfd) {
                    close(events[j].data.fd);
                }

                // 释放锁
                if (ls == 0) {
                    v(sl);
                    ls = -1;
                }
            }
            // 有连接请求
            else if ((events[j].events & EPOLLIN) && (events[j].data.fd == sfd)) {
                struct sockaddr in_addr;
                socklen_t in_len;
                int infd;
                memset(&in_addr, 0, sizeof(struct sockaddr));
                in_len = 1;

                infd = accept(sfd, &in_addr, &in_len);
                if (infd == -1) {
                    printf("process %d accept failed!\n", getpid());
                    printf("\n%d : %s\n", errno, strerror(errno));
                    v(sl);
                    continue;
                }
                ln++;
                printf("process %d accept successed, accepted num: %d\n", getpid(), ln);

                // 将接收的套接字加入监听队列
                event.data.fd = infd;
                event.events = EPOLLIN;
                epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);

                // 删除sfd的监听
                epoll_ctl(efd, EPOLL_CTL_DEL, sfd, NULL);

                // 释放锁
                v(sl);
                ls = -1;
                //sleep(1);
            }
            // 有数据可读
            else if (events[j].events & EPOLLIN) {
                int n = read(events[j].data.fd, buf, 100);
                buf[n] = '\0';
                printf("process %d read : %s\n", getpid(), buf);

                ln--;
                epoll_ctl(efd, EPOLL_CTL_DEL, events[j].data.fd, NULL);
                close(events[j].data.fd);

                if (!strcasecmp(buf, "bye")) {
                    lock_close(sl);
                    exit(0);
                }
            }
        }
    }
}


int main(int argc, char *argv[]) {

    int sfd, i;
    //slock *sl;
    
    // 创建并绑定套接字
    sfd = create_and_bind(5555);
    if (sfd == -1) {
        printf("create_and_bind error\n");
        return -1;
    }

    // 设置套接字为非阻塞模式
    if (set_socket_nonblock(sfd) == -1) {
        printf("set_socket_nonblock error\n");
        return -1;
    }

    if (listen(sfd, SOMAXCONN) < 0 ) {
        printf("listen error\n");
        return -1;
    }

    // 创建一个锁
    sl = lock_init("/thundering-lock-4");
    if (sl == NULL) {
        printf("lock_init error\n");
        return -1;
    }


    // 创建多个进程
    for (i = 0; i < PROCESS_NUM; i++) {
        int pid = fork();

        // 子进程
        if (pid == 0) {
            process_run(sfd, sl);
        }
    }

    // 设置信号处理函数
    addsig(SIGINT);

    int status;
    wait(&status);
    close(sfd);
    //lock_close(sl);

    return 0;
}

