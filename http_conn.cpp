#include "http_conn.h"

/* 初始化静态变量 */
int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

/* 设置文件描述符非阻塞 */
void setnonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    old_flag |= O_NONBLOCK;
    fcntl(fd, F_SETFL, old_flag);
}

/* 向epoll中添加需要监听的文件描述符 */
void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event); //向epoll中添加事件
    setnonblocking(fd); //设置文件描述符非阻塞
}

/* 将文件描述符从epoll中删除 */
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, 0);
    close(fd);
}

/* 修改epoll中的文件描述符 */
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP; //需要重置EPOLLONESHOT事件，确保下次可读时，EPOLLIN事件能够被触发
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/* 初始化新接收的连接 */
void http_conn::init(int sockfd, const sockaddr_in & addr) {
    m_sockfd = sockfd; //方便后续针对该sockfd进行读写操作
    m_addr = addr; //设置地址
    //设置端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    //添加到epoll对象中
    addfd(m_epollfd, m_sockfd, true); //需要设置为EPOLLONESHOT事件
    ++m_user_count; //总用户数加一
}

/* 关闭一个连接 */
void http_conn::close_conn() {
    if (m_sockfd != -1) {
        removefd(m_epollfd, m_sockfd); //将该连接的文件描述符从epoll内核事件表中删除
        m_sockfd = -1; //置为-1后，这个sockfd就没用了
        --m_user_count; //关闭一个连接，客户总数量减少1
    }
}

/* 一次性非阻塞的读出所有数据 */
bool http_conn::read() {
    if (m_read_idx >= READ_BUFFER_SIZE) { //读缓冲区已满
        return false;
    }
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(m_sockfd, m_read_buffer + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break; //没有数据了
            return false;
        } else if (bytes_read == 0) {// 对方关闭连接了
            return false;
        }
        m_read_idx += bytes_read; //更新索引
    }
    printf("读取到了数据：%s\n", m_read_buffer);
    return true;
}

/* 一次性非阻塞的写完所有数据 */
bool http_conn::write() {
    printf("一次性写完数据\n");
    return true;
}

/* 由线程池的工作线程调用，是处理HTTP请求的入口函数，需要做的工作包括：
 * 1. 解析HTTP请求;
 * 2. 生成响应;     */ 
void http_conn::process() {
    printf("parse request\n");
}