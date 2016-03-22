#include "rio.h"
#include "fastcgi.h"
#include <arpa/inet.h>
#include <sys/socket.h>

int main() 
{
	int sock;
	struct sockaddr_in serv_addr;
	int str_len;
	char msg[20];

    // 创建套接字
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (-1 == sock) {
        printf("error \n");
        exit(-1);
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(FCGI_HOST);
	serv_addr.sin_port = htons(FCGI_PORT);

    // 连接服务器
	if(-1 == connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr))){
        printf("error \n");
        exit(-1);
	}


    return 0;
}
