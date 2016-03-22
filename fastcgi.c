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

    free(old);

    if (ret == (FCGI_HEADER_LEN + cl + pl)) {
        return 0;
    } else {
        return -1;
    }
}

/*
 * 发送空的params记录
 * 发送成功返回0
 * 出错返回-1
 */
int sendEmptyParamsRecord(write_record wr, int fd, int requestId)
{
    int ret;
    FCGI_Header nvHeader = makeHeader(FCGI_PARAMS, requestId, 0, 0);
    ret = wr(fd, (char *)&nvHeader, FCGI_HEADER_LEN);

    if (ret == FCGI_HEADER_LEN) {
        return 0;
    } else {
        return -1;
    }
}

/*
 * 发送FCGI_STDIN数据
 * 发送成功返回0
 * 出错返回-1
 */
int sendStdinRecord(
        write_record wr,
        int fd,
        int requestId,
        char *data,
        int len)
{
    int cl = len, pl, ret;
    char buf[8] = {0};

    while (len > 0) {
        // 判断STDIN数据是否大于传输最大值FCGI_MAX_LENGTH
        if (len > FCGI_MAX_LENGTH) {
            cl = FCGI_MAX_LENGTH;
        }

        // 计算填充数据长度
        pl = (cl % 8) == 0 ? 0 : 8 - (cl % 8);

        FCGI_Header sinHeader = makeHeader(FCGI_STDIN, requestId, cl, pl);
        ret = wr(fd, (char *)&sinHeader, FCGI_HEADER_LEN);  // 发送协议头部
        if (ret != FCGI_HEADER_LEN) {
            return -1;
        }

        ret = wr(fd, data, cl); // 发送stdin数据
        if (ret != cl) {
            return -1;
        }

        if (pl > 0) {
            ret = wr(fd, buf, pl); // 发送填充数据
            if (ret != pl) {
                return -1;
            }
        }

        len -= cl;
        data += cl;
    }

    return 0;
}

/*
 * 发送空的FCGI_STDIN记录
 * 发送成功返回0
 * 出错返回-1
 */
int sendEmptyStdinRecord(write_record wr, int fd, int requestId)
{
    int ret;
    FCGI_Header sinHeader = makeHeader(FCGI_STDIN, requestId, 0, 0);
    ret = wr(fd, (char *)&sinHeader, FCGI_HEADER_LEN);

    if (ret == FCGI_HEADER_LEN) {
        return 0;
    } else {
        return -1;
    }
}

/*
 * 读取php-fpm处理结果
 * 读取成功返回0
 * 出错返回-1
 */
int recvResult(
        read_record rr, 
        int fd, 
        int requestId,
        char **sout,
        int *outlen,
        char **serr,
        int *errlen,
        FCGI_EndRequestBody *endRequest)
{
    FCGI_Header responHeader;
    char *conBuf = NULL, *errBuf = NULL;
    int buf[8], cl, ret;

    *outlen = 0;
    *errlen = 0;
    // 读取协议记录头部
    while (rr(fd, &responHeader, FCGI_HEADER_LEN) > 0) {
        if (responHeader.type == FCGI_STDOUT && responHeader.requestId == requestId) {
            // 获取内容长度
            cl = (int)(responHeader.contentLengthB1 << 8) + (int)responHeader.contentLengthB0;
            *outlen += cl;

            // 如果不是第一次读取FCGI_STDOUT记录
            if (conBuf != NULL) { 
                // 扩展空间
                conBuf = realloc(*sout, *outlen);
            } else {
                conBuf = (char *)malloc(cl);
                *sout = conBuf;
            }

            ret = rr(fd, conBuf, cl);
            if (ret == -1 || ret != cl) {
                return -1;
            }

            // 读取填充内容，忽略
            if (responHeader.paddingLength > 0) {
                rr(fd, buf, responHeader.paddingLength);
                if (ret == -1 || ret != responHeader.paddingLength) {
                    return -1;
                }
            }
        } else if (responHeader.type == FCGI_STDERR && responHeader.requestId == requestId) {
            // 获取内容长度
            cl = (int)(responHeader.contentLengthB1 << 8) + (int)responHeader.contentLengthB0;
            *errlen += cl;

            // 如果不是第一次读取FCGI_STDOUT记录
            if (errBuf != NULL) { 
                // 扩展空间
                errBuf = realloc(*serr, *errlen);
            } else {
                errBuf = (char *)malloc(cl);
                *serr = errBuf;
            }

            ret = rr(fd, errBuf, cl);
            if (ret == -1 || ret != cl) {
                return -1;
            }

            // 读取填充内容，忽略
            if (responHeader.paddingLength > 0) {
                rr(fd, buf, responHeader.paddingLength);
                if (ret == -1 || ret != responHeader.paddingLength) {
                    return -1;
                }
            }
        } else if (responHeader.type == FCGI_END_REQUEST && responHeader.requestId == requestId) {
            // 读取结束请求协议体
            ret = rr(fd, endRequest, sizeof(FCGI_EndRequestBody));

            if (ret == -1 || ret != sizeof(FCGI_EndRequestBody)) {
                return -1;
            }
            return 0;
        }
    }
}

