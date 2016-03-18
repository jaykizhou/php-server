#include "rio.h"
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
    char filename[256];     // 请求文件名
    char cgiargs[256];      // 查询参数
    char contype[256];      // 请求体类型
    unsigned int conlength; // 请求体长度
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
    is_static = parse_uri(hhr.uri, hhr.filename, hhr.cgiargs);

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
        serve_dynamic(fd, &hhr);
    }
}

/*
 * 读取请求头部信息
 * 如果是GET请求，则简单忽略
 * 如果是POST请求，则提取请求体类型和长度
 */
void read_requesthdrs(rio_t *rp, hht_t *hp)
{
    char buf[MAXLINE];
    char *start, *end;

    memset(buf, 0, MAXLINE);
    rio_readlineb(rp, buf, MAXLINE);

    while (strcmp(buf, "\r\n")) {

        start = index(buf, ':');
        // 每行数据包含\r\n字符，需要删除
        end = index(buf, '\r');
        *end = '\0';

        if (is_contype(buf)) {
            strcpy(hp->contype, p + 1);
        } else if (is_conlength(buf)) {
            strcpy(hp->conlength, p + 1);
        }

        memset(buf, 0, MAXLINE);
        rio_readlineb(rp, buf, MAXLINE);
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
int parse_uri(char *uri, char *filename, char *cgiargs) 
{                                                        
    char *ptr, *query;;                                   
    char urin[LOCALBUF];
    char *delim = ".php"; // 根据后缀名判断是静态页面还是动态页面

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
        strcpy(filename, ".");                                        
        strcat(filename, urin);                                   
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
void serve_dynamic(int fd, hht_t *hp)
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
