#include "rio.h"
#include "fastcgi.h"
#include "server.h"

/*
 * 请求头部结构体
 * 只存储请求行和类型、长度字段，其他信息忽略
 * 也存储了从请求地址中分析出的请求文件名和查询参数
 */
struct http_header {
    char uri[256];          // 请求地址
    char method[16];        // 请求方法
    char version[16];       // 协议版本
    char filename[256];     // 请求文件名(包含完整路径)
    char name[256];         // 请求文件名(不包含路径，只有文件名)
    char cgiargs[256];      // 查询参数
    char contype[256];      // 请求体类型
    char conlength[16];     // 请求体长度
};

/*
 * 处理一个HTTP事务
 */
void doit(int fd)
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE];
    hhr_t hhr;
    rio_t rio;

	memset(&hhr, 0, sizeof(hhr));
	memset(buf, 0, MAXLINE);

    // 读取请求行
    rio_readinitb(&rio, fd);
    if (rio_readlineb(&rio, buf, MAXLINE) < 0) {
        error_log("readlineb error", DEBUGARGS);
    }

    // 提取请求方法、请求URI、HTTP版本
    sscanf(buf, "%s %s %s", hhr.method, hhr.uri, hhr.version);

    // 只接收GET和POST请求
    if (strcasecmp(hhr.method, "GET") && strcasecmp(hhr.method, "POST")) {
        clienterror(fd, hhr.method, "501", "Not Implement",
                "Zhou does not implement this method");
        return ;
    }

    // 读取请求报头
    read_requesthdrs(&rio, &hhr);

    // 分析请求uri，获得具体请求文件名和请求参数
    is_static = parse_uri(hhr.uri, hhr.filename, hhr.name, hhr.cgiargs);

    // 判断请求文件是否存在
    if (stat(hhr.filename, &sbuf) < 0) {
        clienterror(fd, hhr.filename, "404", "Not found",
                "Zhou couldn't find this file");
        return ;
    }

    if (is_static) { // 静态文件
        // 判断是否是普通文件及是否有读权限
        if (!S_ISREG(sbuf.st_mode) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, hhr.filename, "403", "Forbidden",
                    "Zhou couldn't read the file");
            return ;
        }
        serve_static(fd, hhr.filename, sbuf.st_size);
    } else { // 动态文件
        // 判断是否有执行权限
        if (!S_ISREG(sbuf.st_mode) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, hhr.filename, "403", "Forbidden",
                    "Zhou couldn't run the CGI program");
        }
        serve_dynamic(&rio, &hhr);
    }
}

/*
 * 读取请求头部信息
 * 如果是GET请求，则简单忽略
 * 如果是POST请求，则提取请求体类型和长度
 */
void read_requesthdrs(rio_t *rp, hhr_t *hp)
{
    char buf[MAXLINE];
    char *start, *end;

    memset(buf, 0, MAXLINE);
    if (rio_readlineb(rp, buf, MAXLINE) < 0) {
        error_log("rio_readlineb error", DEBUGARGS);
    }

    while (strcmp(buf, "\r\n")) {

        start = index(buf, ':');
        // 每行数据包含\r\n字符，需要删除
        end = index(buf, '\r');

        if (start != 0 && end != 0) {
            *end = '\0';
            while ((*(start + 1)) == ' ') {
                start++;
            }

            if (is_contype(buf)) {
                strcpy(hp->contype, start + 1);
            } else if (is_conlength(buf)) {
                strcpy(hp->conlength, start + 1);
            }
        }

        memset(buf, 0, MAXLINE);
        if (rio_readlineb(rp, buf, MAXLINE) < 0) {
            error_log("rio_readlineb error", DEBUGARGS);
        }
    }

    return ;
}

/*
 * 分析请求uri，提取具体文件名和查询参数
 * 请求的是静态文件返回1
 * 请求的是动态文件返回0
 * 默认的服务器根目录就是程序所在目录
 * 默认页面是index.html
 */
int parse_uri(char *uri, char *filename, char *name, char *cgiargs) 
{                                                        
    char *ptr, *query;;                                   
    char urin[LOCALBUF];
    char *delim = ".php"; // 根据后缀名判断是静态页面还是动态页面
    char cwd[LOCALBUF];
    char *dir;

    strcpy(urin, uri); // 不破坏原始字符串

    if (!(query = strstr(urin, delim))) { // 静态页面
        strcpy(cgiargs, "");                          

        //  删除无用参数 /index.html?123435
        ptr = index(urin, '?');             
        if (ptr) {
            *ptr = '\0';
        }

        strcpy(filename, ".");                 
        strcat(filename, urin);                
        // 如果以‘/’结尾，自动添加index.html  
        if (urin[strlen(urin) - 1] == '/') {   
            strcat(filename, "index.html"); 
        }                                  
        return 1;                             
    } else { // 动态页面                     
        // 提取查询参数                     
        ptr = index(urin, '?');             
        if (ptr) {                    
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0';        
        } else {                   
            // 类似index.php/class/method会提取class/method的参数     
            if (*(query + sizeof(delim)) == '/') {
                strcpy(cgiargs, query + sizeof(delim) + 1);                              
                *(query + sizeof(delim)) = '\0';
            }
        }                                                          
        dir = getcwd(cwd, LOCALBUF); // 获取当前工作目录
        strcpy(filename, dir);       // 包含完整路径名                                   
        strcat(filename, urin);                                   
        strcpy(name, urin);         // 不包含完整路径名
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
    sprintf(buf, "%sContent-Length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, filetype);
    if (rio_writen(fd, buf, strlen(buf)) < 0) {
        error_log("write to client error", DEBUGARGS);
    }

    if ((srcfd = open(filename, O_RDONLY, 0)) < 0) {
        error_log("open file error", DEBUGARGS);
    }

    if ((srcp = mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0)) == ((void *) -1)) {
        error_log("mmap error", DEBUGARGS);
    }
    close(srcfd);
    if (rio_writen(fd, srcp, filesize) < 0) {
        error_log("wirte to client error", DEBUGARGS);
    }

    if (munmap(srcp, filesize) < 0) {
        error_log("munmap error", DEBUGARGS);
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
void serve_dynamic(rio_t *rp, hhr_t *hp) {
    int sock; 

    // 创建一个连接到fastcgi服务器的套接字
    sock = open_clientfd();

    // 发送http请求数据
    send_fastcgi(rp, hp, sock);

    // 接收处理结果
    recv_fastcgi(rp->rio_fd, sock);

    close(sock); // 关闭与fastcgi服务器连接的套接字
}

/*
 * 接收fastcgi返回的数据
 */
int recv_fastcgi(int fd, int sock) {
    int requestId;
    char *p;
    int n;

    requestId = sock;

    // 读取处理结果
    if (recvRecord(rio_readn, send_to_cli, fd, sock, requestId) < 0) {
        error_log("recvRecord error", DEBUGARGS);
        return -1;
    }

    /*
    FCGI_EndRequestBody endr;
    char *out, *err;
    int outlen, errlen;

    requestId = sock;

    // 读取处理结果
    if (recvRecord(rio_readn, sock, requestId, &out, &outlen, &err, &errlen, &endr) < 0) {
        error_log("recvRecord error", DEBUGARGS);
        return -1;
    }

    if (outlen > 0) {
        p = index(out, '\r'); 
        n = (int)(p - out);
        rio_writen(fd, p + 3, outlen - n - 3);
        free(out);
    }

    if (errlen > 0) {
        rio_writen(fd, err, errlen);
        free(err);
    }
    */

    return 0;
}

/*
 * php处理结果发送给客户端
 */
int send_to_cli(int fd, int outlen, char *out, 
        int errlen, char *err, FCGI_EndRequestBody *endr
        ) 
{
    char *p;
    int n;

    if (outlen > 0) {
        p = index(out, '\r'); 
        n = (int)(p - out);
        if (rio_writen(fd, p + 3, outlen - n - 3) < 0) {
            error_log("rio_written error", DEBUGARGS);
            return -1;
        }
    }

    if (errlen > 0) {
        if (rio_writen(fd, err, errlen) < 0) {
            error_log("rio_written error", DEBUGARGS);
            return -1;
        }
    }
}

/*
 * 发送http请求行和请求体数据给fastcgi服务器
 */
int send_fastcgi(rio_t *rp, hhr_t *hp, int sock)
{
    int requestId, i, l; 
    char *buf;

    requestId = sock;

    // params参数名
    char *paname[] = {
        "SCRIPT_FILENAME",
        "SCRIPT_NAME",
        "REQUEST_METHOD",
        "REQUEST_URI",
        "QUERY_STRING",
        "CONTENT_TYPE",
        "CONTENT_LENGTH"
    };

    // 对应上面params参数名，具体参数值所在hhr_t结构体中的偏移
    int paoffset[] = {
        (size_t) & (((hhr_t *)0)->filename),
        (size_t) & (((hhr_t *)0)->name),
        (size_t) & (((hhr_t *)0)->method),
        (size_t) & (((hhr_t *)0)->uri),
        (size_t) & (((hhr_t *)0)->cgiargs),
        (size_t) & (((hhr_t *)0)->contype),
        (size_t) & (((hhr_t *)0)->conlength)
    };

    // 发送开始请求记录
    if (sendBeginRequestRecord(rio_writen, sock, requestId) < 0) {
        error_log("sendBeginRequestRecord error", DEBUGARGS);
        return -1;
    }

    // 发送params参数
    l = sizeof(paoffset) / sizeof(paoffset[0]);
    for (i = 0; i < l; i++) {
        // params参数的值不为空才发送
        if (strlen((char *)(((int)hp) + paoffset[i])) > 0) {
            if (sendParamsRecord(rio_writen, sock, requestId, paname[i], strlen(paname[i]),
                        (char *)(((int)hp) + paoffset[i]), 
                        strlen((char *)(((int)hp) + paoffset[i]))) < 0) {
                error_log("sendParamsRecord error", DEBUGARGS);
                return -1;
            }
        }
    }

    // 发送空的params参数
    if (sendEmptyParamsRecord(rio_writen, sock, requestId) < 0) {
        error_log("sendEmptyParamsRecord error", DEBUGARGS);
        return -1;
    }

    // 继续读取请求体数据
    l = atoi(hp->conlength);
    if (l > 0) { // 请求体大小大于0
        buf = (char *)malloc(l + 1);
        memset(buf, '\0', l);
        if (rio_readnb(rp, buf, l) < 0) {
            error_log("rio_readn error", DEBUGARGS);
            free(buf);
            return -1;
        }

        // 发送stdin数据
        if (sendStdinRecord(rio_writen, sock, requestId, buf, l) < 0) {
            error_log("sendStdinRecord error", DEBUGARGS);
            free(buf);
            return -1;
        }

        free(buf);
    }

    // 发送空的stdin数据
    if (sendEmptyStdinRecord(rio_writen, sock, requestId) < 0) {
        error_log("sendEmptyStdinRecord error", DEBUGARGS);
        return -1;
    }

    return 0;
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
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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

/*
 * 创建连接fastcgi服务器的客户端套接字
 * 出错返回-1
 */
int open_clientfd() {
    int sock;
	struct sockaddr_in serv_addr;

    // 创建套接字
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (-1 == sock) {
        error_log("socket error", DEBUGARGS);
        return -1;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(FCGI_HOST);
	serv_addr.sin_port = htons(FCGI_PORT);

    // 连接服务器
	if(-1 == connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr))){
        error_log("connect error", DEBUGARGS);
        return -1;
	}

    return sock;
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
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-Type: text/html\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-Length: %d\r\n\r\n", strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
}

/*
 * 打印错误信息
 */
void error_log(const char *msg, const char *filename, 
        int line, const char *func)
{
    fprintf(stderr, "%s(%d)-%s a error happen -> %s：%s\n", filename, line,
            func, msg, strerror(errno));
    exit(0);
}

/*
 * 将str前n个字符转换为小写
 */
static void strtolow(char *str, int n)
{
    char *cur = str;
    while (n > 0) {
        *cur = tolower(*cur);
        cur++;
        n--;
    }
}

/*
 * 判断str起始位置开始是否包含"content-type"
 * 包含返回1
 * 不包含返回0
 */
static int is_contype(char *str)
{
    char *cur = str;
    char *cmp = "content-type";

    // 删除开始的空格
    while (*cur == ' ') {
        cur++;
    } 
    
    for (; *cmp != '\0' && tolower(*cur) == *cmp; cur++,cmp++)
        ;

    if (*cmp == '\0') { // cmp字符串以0结束
        return 1;
    }

    return 0;

}

/*
 * 判断str起始位置开始是否包含"content-length"
 * 包含返回1
 * 不包含返回0
 */
static int is_conlength(char *str)
{
    char *cur = str;
    char *cmp = "content-length";

    // 删除开始的空格
    while (*cur == ' ') {
        cur++;
    } 
    
    for (; *cmp != '\0' && tolower(*cur) == *cmp; cur++,cmp++)
        ;

    if (*cmp == '\0') { // cmp字符串以0结束
        return 1;
    }

    return 0;

}
