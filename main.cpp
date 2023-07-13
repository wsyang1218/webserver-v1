#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h> 
#include <signal.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65535 //最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 //epoll一次监听的最大的事件的数量

/* 设置信号处理函数 */
void addsig(int sig, void( handler )(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

extern int addfd(int epollfd, int fd, bool onshot); /* 添加文件描述符到epoll中 */
extern int removefd(int epollfd, int fd); /* 从epoll中删除文件描述符 */
extern void modfd(int epollfd, int fd, int ev); /* 修改epoll中的文件描述符 */

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printf("请按照如下格式运行: %s port_number\n", basename(argv[0]));
        /* argv[0]是程序的完整路径，basename[0]用于获取路径中的文件名，就是把除了文件名以外的部分删除了。 */
        exit(-1);
    }
    //获取端口号
    int port = atoi(argv[1]);

    //处理SIGPIPE信号
    addsig(SIGPIPE, SIG_IGN); //进程收到SIGPIPE信号的默认行为是终止进程，这里设置成ignore
    
    //创建线程池，初始化线程池
    threadpool<http_conn> * pool = NULL; /* 需要处理的任务是http连接 */
    try {
        pool = new threadpool<http_conn>;
    } catch(...) {
        exit(-1);
    }

    //创建一个数组用于保存所有的客户端信息
    http_conn * users = new http_conn[ MAX_FD ];

    //创建socket -> 设置端口复用 -> 绑定socket -> 设置监听
    int listenfd = socket(PF_INET, SOCK_STREAM, 0); //创建socket
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)); //设置端口复用（注意端口复用的设置时机要在绑定之前）
    struct sockaddr_in addr; //定义socket地址结构体
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY就是指定地址为0.0.0.0的地址，表示本机所有ip，不管数据从哪个网卡来，只要是发送到了绑定的端口号，该socket都可以接收到
    addr.sin_port = htons(port); //host to net (short)函数将主机字节序转换为网络字节序
    bind(listenfd, (struct sockaddr *) &addr, sizeof(addr)); //绑定套接字
    listen(listenfd, 5); //监听socket，5表示内核监听队列的最大长度，一般都设置成5

    //设置io多路复用
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(1); //创建epoll内核事件表，返回一个文件描述符
    //将监听文件描述符添加到epollfd中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd; //所有的http连接共享同一个epollfd

    while (true) {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ( ret < 0 && errno != EINTR ) {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < ret; ++i) {
            int sockfd = events[i].data.fd; /* 获取监听到的文件描述符 */
            if (sockfd == listenfd) {  
                // 有客户端连接进来
                struct sockaddr_in client_addr;
                socklen_t client_addrlen = sizeof(client_addr);
                int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &client_addrlen); //返回连接的fd
                if (http_conn::m_user_count >= MAX_FD ) { /* 当前连接数量满了 */
                    //TODO: 给客户端写一个信息，告诉客户端服务器正忙等。
                    close(connfd); /* 关闭连接fd */
                    continue;
                }
                users[connfd].init(connfd, client_addr);//将新的客户数据初始化放到数组中
                /* 这里直接将connfd作为索引，users已经初始化了但是内容都是默认值 */
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {  //对方异常断开或发送错误等事件
                users[sockfd].close_conn(); //关闭连接
            } else if (events[i].events & EPOLLIN) { //检测到可读事件
                if (users[sockfd].read()) { //一次性读出所有数据
                    pool->append(users + sockfd);
                } else { //读失败了或没有读到数据，就关闭连接
                    users[sockfd].close_conn();
                }
            } else if (events[i].events & EPOLLOUT) {
                if (!users[sockfd].write()) { //一次性写完所有数据，写完后就关闭连接
                    users[sockfd].close_conn();
                }
            }
        }        
    }
}