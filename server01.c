#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080                   // 服务器监听端口

void errorHandling(char *message);  // 错误处理


int main(int argc, char *argv[]){

	int serv_sock;                  // 保存后面创建的服务器套接字
	int clnt_sock;                  // 保存接受请求的客户端套接字

        char buf[1024];                     // 缓冲区

	struct sockaddr_in serv_addr;   // 保存服务器套接字地址信息
	struct sockaddr_in clnt_addr;   // 保存客户端套接字地址信息 
	socklen_t clnt_addr_size;       // 客户端套接字地址变量的大小

        // 发送给客户端的固定内容
	char status[] = "HTTP/1.0 200 OK\r\n";
        char header[] = "Server: A Simple Web Server\r\nContent-Type: text/html\r\n\r\n";
        char body[] = "<html><head><title>A Simple Web Server</title></head><body><h2>Welcome!</h2><p>This is shiyanlou!</p></body></html>";

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

        // 接受客户端的请求
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr *) &clnt_addr, &clnt_addr_size);
        if(-1 == clnt_sock){
           errorHandling("accept() error");
        }

       // 读取客户端请求
       read(clnt_sock, buf, sizeof(buf) -1);
       printf("%s", buf);
       

       // 向客户端套接字发送信息
       write(clnt_sock, status, sizeof(status));
       write(clnt_sock, header, sizeof(header));
       write(clnt_sock, body, sizeof(body));

       // 关闭套接字
      close(clnt_sock);
      close(serv_sock);

      return 0;

}

void errorHandling(char *message){
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}


