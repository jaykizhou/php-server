#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define FCGI_HOST "127.0.0.1"
#define FCGI_PORT 9000
#define FCGI_REQUEST_ID 1
#define FCGI_VERSION_1 1
#define FCGI_BEGIN_REQUEST 1
#define FCGI_RESPONDER 1
#define FCGI_END_REQUEST 3
#define FCGI_PARAMS 4
#define FCGI_STDIN 5
#define FCGI_STDOUT 6
#define FCGI_STDERR 7

/*
 * FastCGI协议包头
 */
typedef struct{
	unsigned char version;
	unsigned char type;
	unsigned char requestIdB1;
	unsigned char requestIdB0;
	unsigned char contentLengthB1;
	unsigned char contentLengthB0;
	unsigned char paddingLength;
	unsigned char reserved;
}FCGI_Header;

/*
 * FCGI_BEGIN_REQUEST请求包体
 */
typedef struct{
	unsigned char roleB1;
	unsigned char roleB0;
	unsigned char flags;
	unsigned char reserved[5];
}FCGI_BeginRequestBody;

/*
 * FCGI_BEGIN_REQUEST请求记录
 */
typedef struct{
	FCGI_Header header;
	FCGI_BeginRequestBody body;
}FCGI_BeginRequestRecord;

/*
 * FCGI_PARAMS传递参数记录录
 */
typedef struct{
	FCGI_Header header;
	unsigned char nameLength;
	unsigned char valueLength;
	unsigned char data[0];
}FCGI_ParamsRecord;

/*
 * 构造请求头部，返回FCGI_Header结构体
 */
FCGI_Header makeHeader(
		int type,
		int requestId,
		int contentLength,
		int paddingLength)
{
	FCGI_Header header;
	header.version = FCGI_VERSION_1;
	header.type             = (unsigned char) type;
	header.requestIdB1      = (unsigned char) ((requestId     >> 8) & 0xff);
	header.requestIdB0      = (unsigned char) ((requestId         ) & 0xff);
	header.contentLengthB1  = (unsigned char) ((contentLength >> 8) & 0xff);
	header.contentLengthB0  = (unsigned char) ((contentLength     ) & 0xff);
	header.paddingLength    = (unsigned char) paddingLength;
	header.reserved         =  0;
	return header;
}

/*
 * 构造请求体，返回FCGI_BeginRequestBody结构体
 */
FCGI_BeginRequestBody makeBeginRequestBody(
		int role)
{
	FCGI_BeginRequestBody body;
	body.roleB1 = (unsigned char) ((role >>  8) & 0xff);
	body.roleB0 = (unsigned char) (role         & 0xff);
	body.flags  = (unsigned char) 0;
	memset(body.reserved, 0, sizeof(body.reserved));
	return body;
}

void errorHandling(char *message){
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(1);
}

int main(int argc, char *argv[]){
	int sock;
	struct sockaddr_in serv_addr;
	int str_len;
	char msg[20];

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
    char *params[][2] = {
        {"SCRIPT_FILENAME", "/home/shiyanlou/php-server/test.php"}, 
        {"REQUEST_METHOD", "GET"}, 
        {"QUERY_STRING", "name=shiyanlou"}, 
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
		printf("Write params %s  %s\n",params[i][0], params[i][1]);
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
	printf("Start read....\n");
	printf("fastcgi responder is : %X\n", respHeader.type);
	printf("fastcgi responder is : %X\n", respHeader.contentLengthB1);
	printf("fastcgi responder is : %X\n", respHeader.contentLengthB0);
    if(respHeader.type == FCGI_STDOUT){
        int contentLengthR = 
            ((int)respHeader.contentLengthB1 << 8) + (int)respHeader.contentLengthB0;
        message = (char *)malloc(contentLengthR);
        read(sock, message, contentLengthR);
        printf("%s",message);
        free(message);
    }

	close(sock);

	return 0;
}



