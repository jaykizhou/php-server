#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "fastcgi.h"

#define PORT 8080                   // 服务器监听端口

void requestHandling(void *sock);   // 浏览器请求处理
void sendData(void *sock, char *filename); // 向浏览器发送请求文件的内容
void catHTML(void *sock, char *filename); // 读取HTML文件内容
void catJPEG(void *sock, char *filename); // 读取图像文件内容
void sendError(void *sock);         // 请求出错响应
void errorHandling(char *message);  // 错误处理
void catPHP(void *sock, char *filename, char *query); // 读取PHP文件内容


int main(int argc, char *argv[]){

	int serv_sock;                  // 保存后面创建的服务器套接字
	int clnt_sock;                  // 保存接受请求的客户端套接字

	struct sockaddr_in serv_addr;   // 保存服务器套接字地址信息
	struct sockaddr_in clnt_addr;   // 保存客户端套接字地址信息 
	socklen_t clnt_addr_size;       // 客户端套接字地址变量的大小

	// 创建一个服务器套接字
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(-1 == serv_sock){
		errorHandling("socket() error");
	}
	
	// 配置套接字IP和端口信息
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(PORT);

    // 绑定服务器套接字
	if(-1 == bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))){
		errorHandling("bind() error");
	}

    // 监听服务器套接字
	if(-1 == listen(serv_sock, 5)){
		errorHandling("listen() error");
	}

    while(1){
        // 接受客户端的请求
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *) &clnt_addr, &clnt_addr_size);
        if(-1 == clnt_sock){
            errorHandling("accept() error");
        }

        // 处理客户端请求
        requestHandling((void *) &clnt_sock);
    }

    // 关闭套接字
    close(serv_sock);

	return 0;

}

/**
 * 处理浏览器请求
 * 参数：客户端套接字地址
 */
void requestHandling(void *sock){
    int clnt_sock = *((int *) sock);
    char buf[1024];          // 缓冲区     
    char method[10];         // 保存请求方法，GET、POST
    char filename[100];       // 保存请求的文件名

    // 读取浏览器请求内容
    read(clnt_sock, buf, sizeof(buf) - 1);

    // 检查请求协议是否正确
    if(NULL == strstr(buf, "HTTP/")){
        sendError(sock);
        close(clnt_sock);
        return ;
    }

    // 提取请求方法至method数组中
    strcpy(method, strtok(buf, " /"));  

    // 提取请求文件名至filename数组中
    strcpy(filename, strtok(NULL, " /"));

    // 判断请求方法是否是GET，不是GET则进行请求错误处理
    if( 0 != strcmp(method, "GET") ){
        sendError(sock);
        close(clnt_sock);
        return ;
    }
    
    // 访问请求文件
    sendData(sock, filename);
}

/**
 * 处理浏览器请求的文件
 * 参数：客户端套接字地址
 *       请求文件名
 */
void sendData(void *sock, char *filename){
    int clnt_sock = *((int *) sock);
    char buf[20];
    char ext[10];
    char query[100];

    // 提取查询字符串
    if(NULL != strstr(filename, "?")){
        strcpy(buf, strtok(filename, "?"));
        strcpy(query, strtok(NULL, "?"));
    }else{
        strcpy(buf, filename);
        strcpy(query, "name=123456");
    }

    // 判断文件类型
    strtok(buf, ".");
    strcpy(ext, strtok(NULL, "."));
    if(0 == strcmp(ext, "php")){ // 如果是php文件
        catPHP(sock, filename, query);
    }else if(0 == strcmp(ext, "html")){  // 如果是html文件
        catHTML(sock, filename);
    }else if(0 == strcmp(ext, "jpg")){ // 如果是jpg图片
        catJPEG(sock, filename);
    }else{
        sendError(sock);
        close(clnt_sock);
        return ;
    }
}


/**
 * 读取HTML文件内容发送
 * 参数：客户端套接字地址
 *       文件名
 */
void catHTML(void *sock, char *filename){
    int clnt_sock = *((int *) sock);
    char buf[1024];
    FILE *fp;

    char status[] = "HTTP/1.0 200 OK\r\n";
    char header[] = "Server: A Simple Web Server\r\nContent-Type: text/html\r\n\r\n";

    write(clnt_sock, status, strlen(status));    // 发送响应报文状态行
    write(clnt_sock, header, strlen(header));    // 发送响应报文消息头

    fp = fopen(filename, "r");
    if(NULL == fp){
        sendError(sock);
        close(clnt_sock);
        errorHandling("opne file failed!");
        return ;
    }

    // 读取文件内容并发送
    fgets(buf, sizeof(buf), fp);
    while (!feof(fp))
    {
        write(clnt_sock, buf, strlen(buf));
        fgets(buf, sizeof(buf), fp);
    }

    fclose(fp);
    close(clnt_sock);
}

/**
 * 读取JPEG文件内容发送
 * 参数：客户端套接字地址
 *       文件名
 */
void catJPEG(void *sock, char *filename){
    int clnt_sock = *((int *) sock);
    char buf[1024];
    FILE *fp;
    FILE *fw;

    char status[] = "HTTP/1.0 200 OK\r\n";
    char header[] = "Server: A Simple Web Server\r\nContent-Type: image/jpeg\r\n\r\n";

    write(clnt_sock, status, strlen(status));     // 发送响应报文状态行
    write(clnt_sock, header, strlen(header));     // 发送响应报文消息头

    // 图片文件以二进制格式打开
    fp = fopen(filename, "rb");
    if(NULL == fp){
        sendError(sock);
        close(clnt_sock);
        errorHandling("opne file failed!");
        return ;
    }

    // 在套接字上打开一个文件句柄
    fw = fdopen(clnt_sock, "w");
    fread(buf, 1, sizeof(buf), fp);
    while (!feof(fp)){
        fwrite(buf, 1, sizeof(buf), fw);
        fread(buf, 1, sizeof(buf), fp);
    }

    fclose(fw);
    fclose(fp);
    close(clnt_sock);
}

/*
 * 浏览器请求错误时发送的响应内容
 * 参数： 客户端套接字地址
 */
void sendError(void *sock){
    int clnt_sock = *((int *) sock);

    char status[] = "HTTP/1.0 400 Bad Request\r\n";
    char header[] = "Server: A Simple Web Server\r\nContent-Type: text/html\r\n\r\n";
    char body[] = "<html><head><title>Bad Request</title></head><body><p>请求出错，请检查！</p></body></html>";

    // 向客户端套接字发送信息
    write(clnt_sock, status, sizeof(status));
    write(clnt_sock, header, sizeof(header));
    write(clnt_sock, body, sizeof(body));
}

/**
 * 错误处理，直接将错误信息发送到stderr
 * 参数：错误提示信息
 */ 
void errorHandling(char *message){
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

/**
 * 读取PHP文件内容发送
 * 参数：客户端套接字地址
 *       文件名
 *       查询字符串
 */
void catPHP(void *sockc, char *filename, char *query){
    int clnt_sock = *((int *) sockc);
    int sock;
    struct sockaddr_in serv_addr;
    int str_len;
    int contentLengthR;
    char msg[50];
    char buf[1024];

    char status[] = "HTTP/1.0 200 OK\r\n";
    char header[] = "Server: A Simple Web Server\r\n";

    // 创建套接字
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if(-1 == sock){
		errorHandling("socket() error");
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(FCGI_HOST);
	serv_addr.sin_port = htons(FCGI_PORT);

    // 连接服务器
	if(-1 == connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr))){
		errorHandling("connetct() error");
	}


    // 首先构造一个FCGI_BeginRequestRecord结构
    FCGI_BeginRequestRecord beginRecord;
    beginRecord.header = 
        makeHeader(FCGI_BEGIN_REQUEST, FCGI_REQUEST_ID, sizeof(beginRecord.body), 0);
    beginRecord.body = makeBeginRequestBody(FCGI_RESPONDER);

    str_len = write(sock, &beginRecord, sizeof(beginRecord));
	if(-1 == str_len){
		errorHandling("Write beginRecord failed!");
	}

    // 传递FCGI_PARAMS参数
    strcpy(msg, "/home/shiyanlou/php-server/");
    strcat(msg, filename);
    char *params[][2] = {
        {"SCRIPT_FILENAME", msg}, 
        {"REQUEST_METHOD", "GET"}, 
        {"QUERY_STRING", query}, 
        {"", ""}
    };

    int i, contentLength, paddingLength;
    FCGI_ParamsRecord *paramsRecordp;
    for(i = 0; params[i][0] != ""; i++){
        contentLength = strlen(params[i][0]) + strlen(params[i][1]) + 2; // 2字节是两个长度值
        paddingLength = (contentLength % 8) == 0 ? 0 : 8 - (contentLength % 8);
        paramsRecordp = (FCGI_ParamsRecord *)malloc(sizeof(FCGI_ParamsRecord) + contentLength + paddingLength);
        paramsRecordp->nameLength = (unsigned char)strlen(params[i][0]);    // 填充参数值
        paramsRecordp->valueLength = (unsigned char)strlen(params[i][1]);   // 填充参数名
        paramsRecordp->header = 
            makeHeader(FCGI_PARAMS, FCGI_REQUEST_ID, contentLength, paddingLength);
        memset(paramsRecordp->data, 0, contentLength + paddingLength);
        memcpy(paramsRecordp->data, params[i][0], strlen(params[i][0]));
        memcpy(paramsRecordp->data + strlen(params[i][0]), params[i][1], strlen(params[i][1]));
        str_len = write(sock, paramsRecordp, 8 + contentLength + paddingLength);

		if(-1 == str_len){
			errorHandling("Write beginRecord failed!");
		}
		//printf("Write params %s  %s\n",params[i][0], params[i][1]);
        free(paramsRecordp);

    }

    // 传递FCGI_STDIN参数
    FCGI_Header stdinHeader;
    stdinHeader = makeHeader(FCGI_STDIN, FCGI_REQUEST_ID, 0, 0);
    write(sock, &stdinHeader, sizeof(stdinHeader));

    // 读取解析FASTCGI应用响应的数据
    FCGI_Header respHeader;
    char *message;
    str_len = read(sock, &respHeader, 8);
	if(-1 == str_len){
		errorHandling("read responder failed!");
	}
	//printf("Start read....\n");
	//printf("fastcgi responder is : %X\n", respHeader.type);
	//printf("fastcgi responder is : %X\n", respHeader.contentLengthB1);
	//printf("fastcgi responder is : %X\n", respHeader.contentLengthB0);
    if(respHeader.type == FCGI_STDOUT){
        contentLengthR = 
            ((int)respHeader.contentLengthB1 << 8) + (int)respHeader.contentLengthB0;
        //printf("conteng length is : %d\n", (int)respHeader.contentLengthB1 << 8);
        //printf("conteng length is : %d\n", (int)respHeader.contentLengthB0);
        //printf("conteng length is : %d\n", contentLengthR);
        message = (char *)malloc(contentLengthR);
        read(sock, message, contentLengthR);
        //printf("%s",message);
    }

    write(clnt_sock, status, strlen(status));    // 发送响应报文状态行
    write(clnt_sock, header, strlen(header));    // 发送响应报文消息头
    write(clnt_sock, message, contentLengthR);

    free(message);
    close(clnt_sock);
}


