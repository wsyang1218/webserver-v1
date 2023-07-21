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
#include "./locker/locker.h"
#include "./threadpool/threadpool.h"
#include "./http/http_conn.h"
#include "./http/http_conn.cpp"
#include "./timer/timer.h"

#define MAX_FD 65535 //最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 //epoll一次监听的最大的事件的数量
#define TIMESLOT 5 //定时时间

static int pipefd[2]; //用于写入和读取信号的管道，fd[0]是读端，fd[1]是写端
static sort_timer_lst timer_lst; //定时器类
static int epollfd = 0;
static http_conn * users = new http_conn[ MAX_FD ]; //创建一个数组用于保存所有的客户端信息

// extern void setnonblocking(int fd); /* 设置文件描述符为非阻塞 */
// extern int addfd(int epollfd, int fd, bool onshot); /* 添加文件描述符到epoll中 */
// extern int removefd(int epollfd, int fd); /* 从epoll中删除文件描述符 */
// extern void modfd(int epollfd, int fd, int ev); /* 修改epoll中的文件描述符 */

/* 信号处理函数 */
void sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0); //将信号发送到管道
    errno = save_errno;
}

/* 设置信号处理函数 */
void addsig(int sig, void( handler )(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

/* 定时器回调函数，删除非活动连接socket上的注册事件，并关闭 */
void cb_func(client_data* user_data) {
    if (!user_data) return;
    users[user_data->sockfd].close_conn();
    printf("close fd %d\n", user_data->sockfd);
}

void timer_handler() {
    timer_lst.tick(); //处理定时器任务实际上就是调用tick信号
    alarm(TIMESLOT); //alarm调用只会引起一次SIGALRM信号，所以要重新定时，以不管出发SIGALRM信号
}

int main(int argc, char* argv[]) {
    if (argc <= 1) {
        printf("请按照如下格式运行: %s port_number\n", basename(argv[0]));
        /* argv[0]是程序的完整路径，basename[0]用于获取路径中的文件名，就是把除了文件名以外的部分删除了。 */
        exit(-1);
    }
    //获取端口号
    int port = atoi(argv[1]);

    //创建线程池，初始化线程池
    threadpool<http_conn> * pool = NULL; /* 需要处理的任务是http连接 */
    try {
        pool = new threadpool<http_conn>;
    } catch(...) {
        exit(-1);
    }

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
    epollfd = epoll_create(5); //创建epoll内核事件表，返回一个文件描述符
    //将监听文件描述符添加到epollfd中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd; //所有的http连接共享同一个epollfd

    //创建管道
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    if (ret < 0) {
        printf("build pipefd failure\n");
    }
    setnonblocking(pipefd[1]);
    addfd(epollfd, pipefd[0], false);

    //设置信号处理函数
    addsig(SIGPIPE, SIG_IGN); //进程收到SIGPIPE信号的默认行为是终止进程，这里设置成ignore
    addsig(SIGALRM, sig_handler);
    addsig(SIGTERM, sig_handler);
    bool stop_server = false;

    //设置定时器
    client_data * users_timer = new client_data[ MAX_FD ];
    bool timeout = false;
    alarm(TIMESLOT);

    while (!stop_server) {
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
                users[connfd].init(connfd, client_addr);//将新的客户数据初始化放到数组中，直接将connfd作为索引
                users_timer[connfd].address = client_addr;
                users_timer[connfd].sockfd = connfd;
                //创建定时器
                util_timer* timer = new util_timer;
                timer->user_data = &users_timer[connfd]; //绑定用户数据
                timer->cb_func = cb_func; //设置回调函数
                time_t cur = time(NULL); //获取当前时间
                timer->expire = cur + 3 * TIMESLOT; //设置超时时间
                users_timer[connfd].timer = timer;
                timer_lst.add_timer(timer); //将定时器加入到链表中
            } else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {//处理信号
                int sig; 
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0); //一次性获取所有信号
                if (ret == -1) {
                    printf("get signal data failed.\n");
                    continue;
                } else if (ret == 0) {
                    continue;
                } else {
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i])
                        {
                        case SIGALRM:
                            timeout = true;
                            //用timeout标记有定时事件需要处理，但是不立即处理，因为定时任务的
                            //优先级比较低，需要优先处理其它任务
                            break;
                        case SIGTERM:
                            stop_server = true;
                        default:
                            break;
                        }
                    }
                }
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {//对方异常断开或发送错误等事件
                users[sockfd].close_conn(); //关闭连接
            } else if (events[i].events & EPOLLIN) {//检测到可读事件
                util_timer* timer = users_timer[sockfd].timer;
                if (users[sockfd].read()) { //一次性读出所有数据
                    pool->append(users + sockfd);
                    //调整该连接对应的定时器，以延迟该连接被关闭的事件
                    if (timer) {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        printf("adjust timer once\n");
                        timer_lst.adjust_timer(timer);
                    }
                } else { //读失败了或没有读到数据，就关闭连接
                    users[sockfd].close_conn();
                    if (timer) {
                        timer_lst.del_timer(timer); //移除对应的定时器
                    }
                }
            } else if (events[i].events & EPOLLOUT) {//检测到可写事件
                util_timer* timer = users_timer[sockfd].timer;
                if (users[sockfd].write()) {//一次性写完所有数据
                    //调整连接对应的定时器，延迟该连接被关闭的事件
                    if (timer) {
                        time_t cur = time(NULL);
                        timer->expire = cur + 3 * TIMESLOT;
                        printf("adjust timer once\n");
                        timer_lst.adjust_timer(timer);
                    }
                } else {
                    users[sockfd].close_conn();
                    if (timer) {
                        timer_lst.del_timer(timer); //移除对应的定时器
                    }
                }
            }
        }
        //最后处理定时事件
        if (timeout) {
            timer_handler();
            timeout = false;
        }
    }
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete [] users;
    return 0;
}