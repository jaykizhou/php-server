#include "rio.h"
#include "fastcgi.h"
#include <arpa/inet.h>
#include <sys/socket.h>

int main() 
{
	int sock, i, requestId = 1;
	struct sockaddr_in serv_addr;
	char msg[20];

    char *params[][2] = {
        {"SCRIPT_FILENAME", "/home/zhou/php-server/test.php"},
        {"REQUEST_METHOD", "GET"},
        {"QUERY_STRING", "name=test"},
        {"",""}
    };

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
        printf("sock error \n");
        exit(-1);
	}

    // 发送开始请求记录
    if (sendBeginRequestRecord(rio_writen, sock, requestId) < 0) {
        printf("sendBeginRequestRecord error\n");
        return -1;
    }

    // 发送params参数
    for (i = 0; params[i][0] != ""; i++) {
        if (sendParamsRecord(rio_writen, sock, requestId, params[i][0], sizeof(params[i][0]),
                        params[i][1], sizeof(params[i][1])) < 0) {
            printf("sendParamsRecord error\n");
            return -1;
        }
    }

    // 发送空的params参数
    if (sendEmptyParamsRecord(rio_writen, sock, requestId) < 0) {
        printf("sendEmptyParamsRecord error\n");
        return -1;
    }

    FCGI_EndRequestBody endr;
    char *out, *err;
    int outlen, errlen;

    // 读取处理结果
    if (recvRecord(rio_readn, sock, requestId, &out, &outlen, &err, &errlen, &endr) < 0) {
        printf("recvResult error\n");
        return -1;
    }

    if (outlen > 0) {
        printf("------%d------\n", outlen);
        printf("%s\n", out);
    }

    return 0;
}
