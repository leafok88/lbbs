#ifndef _TCPLIB_H
#define _TCPLIB_H

#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>

/**
 * tcplib 用到的错误号.
 */
enum TCPLIB_ERRNO
{
	ERR_TCPLIB_CONNECT = -2,	/** 连接失败 */
	ERR_TCPLIB_TIMEOUT = -3,	/** 超时 */
	ERR_TCPLIB_RECVFAILED = -4, /** 接收报文失败 */
	ERR_TCPLIB_SENTFAILED = -5, /** 发送报文失败 */
	ERR_TCPLIB_VERSION = -10,	/** 版本错误 */
	ERR_TCPLIB_HEADER = -11,	/** 报文头格式错误 */
	ERR_TCPLIB_TOOLONG = -12,	/** 报文数据过长 */
	ERR_TCPLIB_OPENFILE = -20,	/** 打开文件错误 */
	ERR_TCPLIB_RESUMEPOS = -21, /** 续传位置错误 */
	ERR_TCPLIB_OTHERS = -100	/** 其他错误，可从 errno 获得错误信息 */
};

/**
 * 非阻塞型的 TCP 连接函数.
 * 这个函数用法与标准的 connect() 函数非常类似，唯一的区别在于用户
 * 可以自己指定 TCP 连接过程的超时时间。如果在这个时间内没能成功地
 * 连接上目标，将返回超时错误。前面三个参数的意义与 connect() 完全
 * 相同，最后一个参数是超时时间。关于这个函数的实现请参考 UNPv1e2
 * 第 411 页。
 *
 * @param sockfd socket 描述符
 * @param saptr 指向存放目标主机地址结构的指针
 * @param salen 地址结构的长度
 * @param msec 连接过程的超时时间，单位毫秒
 * @return ==0 连接成功
 *         <0  连接失败
 * @see connect()
 */
int NonBlockConnectEx(int sockfd, const struct sockaddr *saptr,
					  socklen_t salen, const int msec, const int conn);

#endif /* _TCPLIB_H */
