#include "fastcgi.h"
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
