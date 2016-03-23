#ifndef __FASTCGI_H__
#define __FASTCGI_H__
#include <string.h>
#include <stdlib.h>

#define FCGI_MAX_LENGTH 0xFFFF          // 允许传输的最大数据长度65536
#define FCGI_HOST       "127.0.0.1"     // php-fpm地址
#define FCGI_PORT       9000            // php-fpm监听的端口地址

#define FCGI_VERSION_1  1               // fastcgi协议版本

/*
 * fastcgi协议报头
 */
typedef struct {
	unsigned char version;          // 版本
	unsigned char type;             // 协议记录类型
	unsigned char requestIdB1;      // 请求ID
	unsigned char requestIdB0;
	unsigned char contentLengthB1;  // 内容长度
	unsigned char contentLengthB0;
	unsigned char paddingLength;    // 填充字节长度
	unsigned char reserved;         // 保留字节
} FCGI_Header;

#define FCGI_HEADER_LEN     8       // 协议包头长度

/*
 * 可用于FCGI_Header的type组件的值
 */
#define FCGI_BEGIN_REQUEST  1   // 请求开始记录类型
#define FCGI_ABORT_REQUEST  2
#define FCGI_END_REQUEST    3   // 响应结束记录类型
#define FCGI_PARAMS         4   // 传输名值对数据
#define FCGI_STDIN          5   // 传输输入数据，例如post数据
#define FCGI_STDOUT         6   // php-fpm响应数据输出
#define FCGI_STDERR         7   // php-fpm错误输出
#define FCGI_DATA           8

/*
 * 请求开始记录的协议体
 */
typedef struct {
	unsigned char roleB1;   // web服务器期望php-fpm扮演的角色
	unsigned char roleB0;
	unsigned char flags;    // 控制连接响应后是否立即关闭
	unsigned char reserved[5];
} FCGI_BeginRequestBody;

/*
 * 开始请求记录结构，包含开始请求协议头和协议体
 */
typedef struct {
	FCGI_Header header;
	FCGI_BeginRequestBody body;
} FCGI_BeginRequestRecord;

/*
 * 期望php-fpm扮演的角色值
 */
#define FCGI_RESPONDER  1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER     3

// 为1，表示php-fpm响应结束不会关闭该请求连接
#define FCGI_KEEP_CONN  1

/*
 * 结束请求记录的协议体
 */
typedef struct {
    unsigned char appStatusB3;
    unsigned char appStatusB2;
    unsigned char appStatusB1;
    unsigned char appStatusB0;
    unsigned char protocolStatus;   // 协议级别的状态码
    unsigned char reserved[3];
} FCGI_EndRequestBody;

/*
 * 协议级别状态码的值
 */
#define FCGI_REQUEST_COMPLETE   1   // 正常结束
#define FCGI_CANT_MPX_CONN      2   // 拒绝新请求，无法并发处理
#define FCGI_OVERLOADED         3   // 拒绝新请求，资源超负载
#define FCGI_UNKNOWN_ROLE       4   // 不能识别的角色

/*
 * 结束请求记录结构
 */
typedef struct {
    FCGI_Header header;
    FCGI_EndRequestBody body;
} FCGI_EndRequestRecord;

typedef struct{
	FCGI_Header header;
	unsigned char nameLength;
	unsigned char valueLength;
	unsigned char data[0];
}FCGI_ParamsRecord;

/*
 * 构造协议记录头部
 */
FCGI_Header makeHeader(
		int type,
		int requestId,
		int contentLength,
		int paddingLength);

/*
 * 构造开始请求记录协议体
 */
FCGI_BeginRequestBody makeBeginRequestBody(
		int role,
        int keepConn);

// 发送协议记录函数指针
typedef ssize_t (*write_record)(int, void *, size_t); 

/*
 * 发送开始请求记录
 */
int sendBeginRequestRecord(write_record wr, int fd, int requestId);

/*
 * 发送名值对参数
 */
int sendParamsRecord(
        write_record wr,
        int fd,
        int requestId,
        char *name,
        int nlen,
        char *value,
        int vlen);

/*
 * 发送空的params记录
 */
int sendEmptyParamsRecord(write_record wr, int fd, int requestId);

/*
 * 发送FCGI_STDIN数据
 */
int sendStdinRecord(
        write_record wr,
        int fd,
        int requestId,
        char *data,
        int len);

/*
 * 发送空的FCGI_STDIN记录
 */
int sendEmptyStdinRecord(write_record wr, int fd, int requestId);

// 读取协议记录函数指针
typedef ssize_t (*read_record)(int, void *, size_t); 

/*
 * 读取php-fpm处理结果
 */
int recvRecord(
        read_record rr,
        int fd,
        int requestId,
        char **sout,
        int *outlen,
        char **serr,
        int *errlen,
        FCGI_EndRequestBody *endRequest);


#endif
