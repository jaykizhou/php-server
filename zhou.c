/*
 * zhou.c 一个简单的web服务器
 * email: yiqianniyiye@126.com
 */
#include "server.h"

int main(int argc, char *argv[])
{
    int listenfd, connfd, port, clientlen;
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

    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = accept(listenfd, (SA *)&clientaddr, &clientlen);
        if (connfd < 0) {
            error_log("accept error", DEBUGARGS);
        }
        doit(connfd);
        close(connfd);
    }

    return 0;
}
