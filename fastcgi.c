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
int sendBeginRequestRecord(write_record wr, int fd, int requestId)
{
    int ret;
    // 构造一个FCGI_BeginRequestRecord结构
    FCGI_BeginRequestRecord beginRecord;

    beginRecord.header = 
        makeHeader(FCGI_BEGIN_REQUEST, requestId, sizeof(beginRecord.body), 0);
    beginRecord.body = makeBeginRequestBody(FCGI_RESPONDER, 0);

    ret = wr(fd, &beginRecord, sizeof(beginRecord));

    if (ret == sizeof(beginRecord)) {
        return 0;
    } else {
        return -1;
    }
}

/*
 * 发送名值对参数
 * 发送成功返回0
 * 出错返回-1
 */
int sendParamsRecord(
        write_record wr,
        int fd,
        int requestId,
        char *name,
        int nlen,
        char *value,
        int vlen)
{
    unsigned char *buf, *old;
    int ret, pl,  cl = nlen + vlen;
    cl = (nlen < 128) ? ++cl : cl + 4; 
    cl = (vlen < 128) ? ++cl : cl + 4; 

    // 计算填充数据长度
    pl = (cl % 8) == 0 ? 0 : 8 - (cl % 8);
    old = buf = (unsigned char *)malloc(FCGI_HEADER_LEN + cl + pl);

    FCGI_Header nvHeader = makeHeader(FCGI_PARAMS, requestId, cl, pl);
    memcpy(buf, (char *)&nvHeader, FCGI_HEADER_LEN);
    buf = buf + FCGI_HEADER_LEN;

    if (nlen < 128) { // name长度小于128字节，用一个字节保存长度
        *buf++ = (unsigned char)nlen;
    } else { // 大于等于128用4个字节保存长度
        *buf++ = (unsigned char)((nlen >> 24) | 0x80);
        *buf++ = (unsigned char)(nlen >> 16);
        *buf++ = (unsigned char)(nlen >> 8);
        *buf++ = (unsigned char)nlen;
    }

    if (vlen < 128) { // value长度小于128字节，用一个字节保存长度
        *buf++ = (unsigned char)vlen;
    } else { // 大于等于128用4个字节保存长度
        *buf++ = (unsigned char)((vlen >> 24) | 0x80);
        *buf++ = (unsigned char)(vlen >> 16);
        *buf++ = (unsigned char)(vlen >> 8);
        *buf++ = (unsigned char)vlen;
    }

    memcpy(buf, name, nlen);
    buf = buf + nlen;
    memcpy(buf, value, vlen);

    ret = wr(fd, old, FCGI_HEADER_LEN + cl + pl);

    if (ret == (FCGI_HEADER_LEN + cl + pl)) {
        return 0;
    } else {
        return -1;
    }
}


