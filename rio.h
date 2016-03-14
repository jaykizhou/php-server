#ifndef __RIO_H__
#define __RIO_H__

#include <stdio.h>
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

#endif
