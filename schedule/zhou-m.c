/*
 * zhou.c 一个简单的web服务器(多进程)
 * email: yiqianniyiye@126.com
 */
#include "server.h"
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
#define MAX_EVENTS 1024

slock *sl;  // 锁结构指针

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
        error_log("epoll_create error", DEBUGARGS);
        return -1;
    }

    // 事件循环
    while (1) {
        /*
         * 判断连接数是否为0，为0,阻塞等待
         * 不为0,非阻塞等待锁
         */

        // 判断是否已经获得了锁
        if (ls == -1) { // 未获得锁
            if (ln > 0) {
                ls = p(sl, 0);
            } else {
                // 阻塞等待锁
                ls = p(sl, 1);
                if (ls == -1) {
                    continue;
                }
            }

            if (ls == 0) {
                // 获取到锁，将监听套接字sfd加入epoll中
                event.data.fd = sfd;
                event.events = EPOLLIN;
                if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event) < 0) {
                    v(sl);
                    ls = -1;
                    if (ln == 0) {
                        continue;
                    }
                }
            }
        }

        int j, n;
        n = epoll_wait(efd, events, MAX_EVENTS, -1);
        if (n < 1) {
            error_log("epoll_wait error", DEBUGARGS);
            return -1;
        }

        for (j = 0; j < n; j++) {

            if ((events[j].events & EPOLLERR) || (events[j].events & EPOLLHUP) || 
                    !(events[j].events & EPOLLIN)) {

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
                    error_log("accept error", DEBUGARGS);
                    v(sl);
                    continue;
                }
                ln++;

                // 将接收的套接字加入监听队列
                event.data.fd = infd;
                event.events = EPOLLIN;
                epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);

                // 删除sfd的监听
                epoll_ctl(efd, EPOLL_CTL_DEL, sfd, NULL);

                // 释放锁
                v(sl);
                ls = -1;
            }
            // 有数据可读
            else if (events[j].events & EPOLLIN) {
                doit(events[j].data.fd);

                ln--;
                epoll_ctl(efd, EPOLL_CTL_DEL, events[j].data.fd, NULL);
                close(events[j].data.fd);

            }
        }
    }
}


int main(int argc, char *argv[])
{
    int i, listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;

    // 检查是否指定了监听端口
    if (argc != 2) {
        port = PORT; // 使用默认端口
    } else {
        port = atoi(argv[1]);
    }

    listenfd = open_listenfd(port);
    if (listenfd < 0) {
        error_log("open_listenfd error", DEBUGARGS);
    }

    // 创建一个锁
    sl = lock_init("/php-server-lock");
    if (sl == NULL) {
        error_log("lock_init error", DEBUGARGS);
        return -1;
    }

    // 创建多个进程
    for (i = 0; i < PROCESS_NUM; i++) {
        int pid = fork();

        // 子进程
        if (pid == 0) {
            process_run(listenfd, sl);
        }
    }

    // 设置信号处理函数
    addsig(SIGINT);

    int status;
    wait(&status);
    close(listenfd);
 
    return 0;
}
