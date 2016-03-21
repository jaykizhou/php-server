#include "fastcgi.h"

/*
 * 构造协议记录头部，返回FCGI_Header结构体
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
 * 构造请求开始记录协议体，返回FCGI_BeginRequestBody结构体
 */
FCGI_BeginRequestBody makeBeginRequestBody(int role, int keepConn)
{
	FCGI_BeginRequestBody body;
	body.roleB1 = (unsigned char) ((role >>  8) & 0xff);
	body.roleB0 = (unsigned char) (role         & 0xff);
	body.flags  = (unsigned char) ((keepConn) ? 1 : 0); // 1为长连接，0为短连接
	memset(body.reserved, 0, sizeof(body.reserved));
	return body;
}

/*
 * 发送开始请求记录
 * 发送成功返回0
 * 出错返回-1
 */
int sendBeginRequestRecord(write_record wr)
{
    int ret;
    // 构造一个FCGI_BeginRequestRecord结构
    FCGI_BeginRequestRecord beginRecord;

    beginRecord.header = 
        makeHeader(FCGI_BEGIN_REQUEST, FCGI_REQUEST_ID, sizeof(beginRecord.body), 0);
    beginRecord.body = makeBeginRequestBody(FCGI_RESPONDER, 0);

    ret = wr(sock, &beginRecord, sizeof(beginRecord));

    if (ret == sizeof(beginRecord)) {
        return 0;
    } else {
        return -1;
    }
	
}
