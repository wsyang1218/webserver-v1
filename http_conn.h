#ifndef HTTPCONNECTION_H
#define HTTPCONNECTION_H

#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include "locker.h"

/* 定义HTTP连接任务类 */

class http_conn {
public:
    static int m_epollfd; /* 所有socket上的事件都被注册到同一个epoll对象中 */
    static int m_user_count; /* 统计用户的数量，有客户端连接+1，有客户端断开-1*/
    static const int READ_BUFFER_SIZE = 2048; //读缓冲区大小
    static const int WRITE_BUFFER_SIZE = 2048; //写缓冲区大小
public:
    http_conn() {}
    ~http_conn() {}
    void process(); /* 处理客户端请求 */
    void init(int sockfd, const sockaddr_in & addr); /* 初始化新接收的连接 */
    void close_conn(); /* 关闭连接 */
    bool read(); /* 一次性读出所有数据，非阻塞的读 */
    bool write(); /* 一次性写完所有数据，非阻塞的写 */
private:
    int m_sockfd; /* 当前HTTP连接的socket */    
    sockaddr_in m_addr; /* 保存客户端的socket地址 */
    char m_read_buffer[READ_BUFFER_SIZE]; /* 读缓冲区 */
    int m_read_idx; /* 标识读缓冲区已经读入的客户端数据的最后一个字节的下一个位置 */
    
};

#endif
