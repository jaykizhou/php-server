#include "rio.h"
#include "server.h"

/*
 * 处理一个HTTP事务
 */
void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    // 读取请求行
    rio_readinitb(&rio, fd);
    if (rio_readlineb(&rio, buf, MAXLINE) < 0) {
        error_log("readlineb error");
    }

    // 提取请求方法、请求URI、HTTP版本
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not Implement",
                "Zhou does not implement this method");
        return ;
    }

    // 读取请求报头
    read_requesthdrs(&rio);

    // 分析请求uri，获得具体请求文件名和请求参数
    is_static = parse_uri(uri, filename, cgiargs);

    // 判断请求文件是否存在
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found",
                "Zhou couldn't find this file");
        return ;
    }

    if (is_static) { // 静态文件
        // 判断是否是普通文件及是否有读权限
        if (!S_ISREG(sbuf.st_mode) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                    "Zhou couldn't read the file");
            return ;
        }
        serve_static(fd, filename, sbuf.st_size);
    } else { // 动态文件
        // 判断是否有执行权限
        if (!S_ISREG(sbuf.st_mode) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden",
                    "Zhou couldn't run the CGI program");
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

/*
 * 读取请求头部信息，目前只是简单忽略
 */
void read_requesthdrs(rio_t *ro)
{
    char buf[MAXLINE];

    rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        rio_readlineb(rp, buf, MAXLINE);
    }

    return ;
}

/*
 * 分析请求uri，提取具体文件名和请求参数
 * 请求的是静态文件返回1
 * 请求的是动态文件返回0
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, ".php")) { // 静态页面
        strcpy(cgiargs, "");
        strcpy(filename, ".");
        strcat(filename, uri);

        // 如果以‘/’结尾，自动添加index.html
        if (uri[strlen(uri) - 1] == '/') {
            strcat(filename, "index.html");
        }
        return 1;
    } else { // 动态页面
        // 提取查询参数
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';
        } else {
            strcpy(cgiargs, "");
        }
        strcpy(filename, ".");
        strcat(filename, uri);
        return 0;
    }
}

/*
 * 读取静态文件内容，发送给客户端
 */
void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXLINE];

    // 获取文件MIME类型
    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    sprintf(buf, "%sServer: Zhou Web Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\b", buf, filetype);
    if (rio_written(fd, buf, strlen(buf)) < 0) {
        error_log("write to client error");
    }

    if ((srcfd = open(filename, O_RDONLY, 0)) < 0) {
        error_log("open file error");
    }

    if ((srcp = mmap(0, fielsize, PROT_READ, MAP_PRIVATE, srcfd, 0)) == ((void *) -1)) {
        error_log("mmap error");
    }
    close(srcfd);
    if (rio_written(fd, srcp, filesize) < 0) {
        error_log("wirte to client error");
    }

    if (munmap(srcp, filesize) < 0) {
        error_log("munmap error");
    }
}

/*
 * 根据文件后缀名判断文件类型
 */
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html")) {
        strcpy(filetype, "text/html");
    }
    else if (strstr(filename, ".gif")) {
        strcpy(filetype, "image/gif");
    }
    else if (strstr(filename, ".jpg")) {
        strcpy(filetype, "image/jpeg");
    }
    else if (strstr(filename, ".png")) {
        strcpy(filetype, "image/png");
    }
    else {
        strcpy(filetype, "text/plain");
    }
}

/*
 * 处理动态文件请求
 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    //....
}

/*
 * 创建一个套接字，然后在指定端口监听
 * 返回监听套接字，出错返回-1
 */
int open_listenfd(int port)
{
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;

    // 创建一个套接字
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) <０) {
        return -1;
    }

    // 设置重启后可以重新使用监听端口
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                (const void *)&optval, sizeof(int)) < 0) {
        return -1;
    }

    // listenfd可以接受port端口上来自任何地址的请求
    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);

    if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0) {
        return -1;
    }

    // 等待请求连接
    if (listen(listenfd, LISTENQ) < 0) {
        return -1;
    }

    return listenfd;
}

void clienterror(int fd, char *cause, char *errnum,
        char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    // 构建http响应体
    sprintf(body, "<html><title>Server Error</title>");
    sprintf(body, "%s<body style='color:red;'>\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s</p>\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr/><em>The Zhou Web Server</em>\r\n</body></html>", body);

    // 发送http响应
    sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    rio_written(fd, buf, strlen(buf));
    sprintf(buf, "Content-Type: text/html\r\n");
    rio_written(fd, buf, strlen(buf));
    sprintf(buf, "Content-Length: %d\r\n\r\n", strlen(body));
    rio_written(fd, buf, strlen(buf));
    rio_written(fd, body, strlen(body));
}

void error_log(char *msg)
{
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}
