#ifndef __RIO_H__
#define __RIO_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define RIO_BUFSIZE 8192
typedef struct {
    int rio_fd;                 // 内部缓冲区对应的描述符
    int rio_cnt;                // 可以读取的字节数
    char *rio_bufptr;           // 下一个可以读取的字节地址 
    char rio_buf[RIO_BUFSIZE];  // 内部缓冲区
} rio_t;

ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);
void rio_readinitb(rio_t *rp, int fd);
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n);
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen);
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n);

#endif
