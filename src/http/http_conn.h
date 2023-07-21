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
#include <string.h>
#include "../locker/locker.h"

/* 定义HTTP连接任务类 */

class http_conn {
public:
    static int m_epollfd; /* 所有socket上的事件都被注册到同一个epoll对象中 */
    static int m_user_count; /* 统计用户的数量，有客户端连接+1，有客户端断开-1*/
    static const int READ_BUFFER_SIZE = 2048; /* 读缓冲区大小 */
    static const int WRITE_BUFFER_SIZE = 2048; /* 写缓冲区大小 */
    static const int FILENAME_LEN = 200; /* 文件名的最大长度 */
    /* http请求方法（目前仅支持GET）*/
    enum METHOD { GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT };
    /* 解析客户端请求时，主状态机的状态，包括：当前正在分析请求行、当前正在分析请求头、当前正在分析请求体 */
    enum CHECK_STATE { CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT };
    /* 解析一行时，从状态机的状态，包括：读取到一个完整行、行出错、行数据尚不完整 */
    enum LINE_STATUS { LINE_OK = 0, LINE_BAD, LINE_OPEN };
    /* 服务器处理http请求的可能结果，即报文解析的结果 */
    enum HTTP_CODE { 
        NO_REQUEST,         //请求不完整，需要继续读取客户数据
        GET_REQUEST,        //表示获得了一个完整的客户请求
        BAD_REQUEST,        //表示客户请求语法错误
        NO_RESOURCE,        //表示服务器没有资源
        FORBIDDEN_REQUEST,  //表示客户对资源没有访问权限
        FILE_REQUEST,       //文件请求，获取文件成功
        INTERNAL_ERROR,     //服务器内部错误
        CLOSED_CONNECTION,  //表示客户端已经关闭连接了
    };
public:
    http_conn() {}
    ~http_conn() {}
    void process(); /* 处理客户端请求 */
    void init(int sockfd, const sockaddr_in & addr); /* 初始化新接收的连接 */
    void close_conn(); /* 关闭连接 */
    bool read(); /* 一次性读出所有数据，非阻塞的读 */
    bool write(); /* 一次性写完所有数据，非阻塞的写 */
private:
    void init(); /* 初始化其他信息 */
    void unmap(); /* 释放内存映射去区 */
    //解析http请求所需函数
    HTTP_CODE process_read(); /* 解析http请求 */
    HTTP_CODE parse_request_line(char * text); /* 解析请求首行 */
    HTTP_CODE parse_headers(char * text);      /* 解析请求头 */
    HTTP_CODE parse_content(char * text);      /* 解析请求体 */
    LINE_STATUS parse_line(); /* 解析一行 */
    char * get_line() {return m_read_buffer + m_start_line; } /* 获取一行数据 */
    HTTP_CODE do_request(); /* 处理请求内容 */
    //生成响应所需函数
    bool process_write(HTTP_CODE ret); /* 根据服务器处理HTTP请求的结果，决定返回给客户端的内容 */
    bool add_response(const char* format, ...); /* 往写缓冲区写入带发送数据 */
    bool add_status_line(int status, const char* title); /* 添加响应状态行 */
    bool add_headers(int content_len); /* 添加响应头部 */
    bool add_content_length(int content_len); /* 添加Content-Length头部信息 */
    bool add_content_type(); /* 添加Content-Type头 */
    bool add_linger(); /* 添加Connection头 */
    bool add_blank_line(); /* 添加空行 */
    bool add_content(const char* content); /* 添加响应体 */
private:
    //连接socket相关信息
    int m_sockfd; /* 当前HTTP连接的socket */    
    sockaddr_in m_addr; /* 保存客户端的socket地址 */
    //数据读写所需参数
    char m_read_buffer[READ_BUFFER_SIZE]; /* 读缓冲区 */
    int m_read_idx; /* 标识读缓冲区已经读入的客户端数据的最后一个字节的下一个位置 */
    char m_write_buffer[WRITE_BUFFER_SIZE]; /* 写缓冲区 */
    int m_write_idx; /* 标识写缓冲区已经写入的数据的最后一个字节的位置 */
    //解析http请求所需参数
    int m_checked_idx; /* 当前正在分析的字符在读缓冲区的位置 */
    int m_start_line; /* 当前正在解析的行的起始位置 */
    CHECK_STATE m_check_state; /* 主状态机当前所处的状态 */
    //存储http请求结果
    char * m_url; /* 请求目标文件的文件名 */
    char * m_version; /* 协议版本，仅支持HTTP1.1 */
    METHOD m_method; /* 请求方法 */
    char * m_host; /* 主机名 */
    bool m_linger; /* 判断http请求是否要保持连接 */
    int m_content_length; /* 请求体长度 */
    //操作http请求资源
    char m_real_file[FILENAME_LEN]; /* 真实的文件地址 */
    struct stat m_file_stat; /* 目标文件的状态，可用于判断文件是否存在、是否为目录、是否可读，并获取文件大小等信息 */
    char * m_file_address; /* 客户请求的目标文件被mmap到内存中的起始位置 */
    //分散读的参数
    struct iovec m_iv[2]; /* 分散写需要的内存块数组 */
    int m_iv_count; /* 被写的内存块数量 */
    //发送数据
    int bytes_to_send; /* 将要发送的数据的字节数 */
    int bytes_have_send; /* 已经发送的字节数 */
};

#endif
