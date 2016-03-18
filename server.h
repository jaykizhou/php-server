#ifndef __SERVER_H__
#define __SERVER_H__
#include "rio.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAXLINE 8192 // 最大行长度
#define MAXBUF 8192  // io缓冲区最大值
#define LOCALBUF 1024 // 局部缓冲区大小

typedef struct sockaddr SA;
typedef struct http_header hhr_t;

#define LISTENQ 1024 // listen函数的第二个参数
int open_listenfd(int port);

#define PORT 8000 // 默认端口号

// error_log函数的最后三个参数
#define DEBUGARGS __FILE__,__LINE__,__FUNCTION__ 
// 错误信息处理
void error_log(const char *msg, const char *filename, 
        int line, const char *func); 

void doit(int fd); // 处理一个http事务
void read_requesthdrs(rio_t *rp, hhr_t *hp); // 读取请求行

// 分析uri
int parse_uri(char *uri, char *filename, char *cgiargs);

// 静态文件请求处理
void serve_static(int fd, char *filename, int filesize);

// 获取请求文件MIME类型
void get_filetype(char *filename, char *filetype);

// 动态文件请求处理
void serve_dynamic(int fd, hhr_t *hp);

/*
 * 向客户端发送一个HTTP响应，其中包含相应的状态码和状态信息
 * 响应主体html文件包含具体错误信息
 */
void clienterror(int fd, char *cause, char *errnum,
        char *shortmsg, char *longmsg);

// 将str前n个字符转换为小写
void strtolow(char *str, int n);

#endif
