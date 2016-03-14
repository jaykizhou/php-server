#include <stdio.h>
#include "rio.h"

/*
 * 从描述符fd的当前文件位置最多传送n个字节到存储器位置usrbuf
 */
ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nread = read(fd, bufp, nleft)) < 0) {
            if (errno == EINTR) { // 被信号处理函数中断返回
                nread = 0;
            } else {
                return -1;  
            }
        } else if (nread == 0) { // EOF
            break;
        }
        nleft -= nread;
        bufp += nread;
    }

    return (n - nleft); // 返回已经读取的字节数
}

/*
 * 将usrbuf缓冲区中的前n个字节数据写入fd中
 * 该函数会保证n个字节都会写入fd中
 */
ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp = usrbuf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR) { // 被信号处理函数中断返回
                nwritten = 0;
            } else { // write函数出错
                return -1;
            }
        }
        nleft -= nwritten;
        bufp += nwritten;
    }

    return n;
}

/*
 * 初始化缓冲区rio_t结构
 */
void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}

static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    while (rp->rio_cnt <= 0) { // 缓冲区为空，继续读取数据到缓冲区
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));

        if (rp->rio_cnt < 0) { // 返回-1
            if (errno != EINTR) {
                return -1;
            }
        } else if (rp->rio_cnt == 0) { // EOF
            return 0;
        } else { 
            rp->rio_bufptr = rp->rio_buf;
        }
    }

    // 比较调用所需的字节数n与内部缓冲区剩余的字节数rp->rio_cnt
    // 取其中最小值
    cnt = n;
    if (rp->rio_cnt < n) {
        cnt = rp->rio_cnt;
    }
    memcpy(usrbuf, rp->rio_buffer, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt = cnt;

    return cnt;
}

/*
 * 从读缓冲区取一行数据
 */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) {
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n') { // 读完了一行
                break;
            }
        } else if (rc == 0) {
            if (n == 1) {
                return 0; // EOF,但没有读取任何数据
            } else {
                break; //EOF,但已经读取了一些数据
            }
        } else { // 出错
            return -1;
        }
    }

    *bufp = 0;
    return n;
}

/*
 * 读取n字节数据
 */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while(nleft > 0) {
        if((nread = rio_read(rp, bufp, nleft)) < 0) {
            if(errno == EINTR) { // 被信号处理程序中断返回
                nread = 0;
            } else {
                return -1; // 读取数据出错
            }
        } else if(nread == 0) { // EOF
            break; 
        } 
        nleft -= nread;
        bufp += nread;
    }

    return (n - nleft);
}
