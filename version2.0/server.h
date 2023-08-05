#ifndef SERVER_H
#define SERVER_H

#include <assert.h>
#include "./logger/logger.h"
#include "./http/http_conn.h"
#include "./threadpool/threadpool.h"
#include "./timer/lst_timer.h"
#include "./utils/utils.h"

using namespace std;

const int MAX_FD = 65535;           //最大的文件描述符个数
const int MAX_EVENT_NUMBER = 10000; //epoll一次监听的最大的事件的数量
const int TIMESLOT = 5;             //定时时间


class Server {
public:
    Server( int port );
    ~Server();
    void init_log(char * log_path, bool is_async);
    void create_threadpool();
    void set_events();
    void start();
private:
    void timer_handler();
    void send_error(int fd, const char * info);
    void add_client(int fd, sockaddr_in & addr);
    void deal_listen_event();
    void deal_read_event(int fd);
    void deal_write_event(int fd); 
    void deal_sig_event();

private:
    int pipefd[2]; //用于写入和读取信号的管道，fd[0]是读端，fd[1]是写端
    http_conn * users; //用于存放用户的数组
    // 管理服务器运行状态
    bool stop_server;
    // 服务器
    int port; //服务器端口
    int listenfd; //监听文件描述符
    threadpool<http_conn> *pool; //线程池
    // 日志功能
    // static int m_close_log; //是否开启日志功能
    // epoll
    epoll_event events[ MAX_EVENT_NUMBER ];//epoll事件列表
    int epollfd; //epoll文件描述符
    // 定时器
    client_data *users_timer; //定时器中存放用户信息的数组
    sort_timer_lst timer_lst;
    bool timeout; // 标志是否发生了定时事件，若为 true 表示发生了定时时间
    // 其它
    Utils utils;
};

#endif