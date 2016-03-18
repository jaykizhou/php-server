#ifndef __FASTCGI_H__
#define __FASTCGI_H__
#include <string.h>

#define FCGI_HOST "127.0.0.1"
#define FCGI_PORT 9000      // php-fpm监听的端口地址
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
 * fastcgi协议报头
 */
typedef struct {
	unsigned char version;          // 版本
	unsigned char type;             // 操作类型
	unsigned char requestIdB1;      // 请求ID
	unsigned char requestIdB0;
	unsigned char contentLengthB1;  // 内容长度
	unsigned char contentLengthB0;
	unsigned char paddingLength;    // 填充字节长度
	unsigned char reserved;         // 保留字节
} FCGI_Header;


typedef struct{
	unsigned char roleB1;
	unsigned char roleB0;
	unsigned char flags;
	unsigned char reserved[5];
}FCGI_BeginRequestBody;

typedef struct{
	FCGI_Header header;
	FCGI_BeginRequestBody body;
}FCGI_BeginRequestRecord;

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
		int paddingLength);

/*
 * 构造请求体，返回FCGI_BeginRequestBody结构体
 */
FCGI_BeginRequestBody makeBeginRequestBody(
		int role);


#endif
