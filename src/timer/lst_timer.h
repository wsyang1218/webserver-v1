#ifndef LST_TIMER_H
#define LST_TIMER_H

#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>

// #define BUFFER_SIZE 64
class util_timer; /* 前向声明 */

/* 用户数据结构，包括客户端socket地址、socket文件描述符、读缓存和定时器*/
struct client_data {
    sockaddr_in address; /* 客户端socket地址 */
    int sockfd;          /* socket文件描述符 */
    // char buf[ BUFFER_SIZE ]; /* 读缓存*/
    util_timer* timer;      /* 定时器 */
};

/* 定时器类 */
class util_timer {
public:
    util_timer() : prev(NULL), next(NULL) {}
public:
    time_t expire; /* 任务超时时间，这里使用绝对时间 */
    void (* cb_func)(client_data *); /* 任务回调函数 */
    client_data* user_data; /* 回到函数处理的客户数据，由定时器的执行者传递给回调函数 */
    util_timer* prev; /* 指向前一个定时器 */
    util_timer* next; /* 指向下一个定时器 */
};

/* 定时器链表，是一个升序、双向链表，且带有头节点和尾节点 */
class sort_timer_lst {
public:
    sort_timer_lst() : head(NULL), tail(NULL) {}
    ~sort_timer_lst();  /* 链表被销毁时，需要删除其中所有的定时器 */
    void add_timer(util_timer* timer);    /* 将目标定时器timer加入到链表中 */
    void adjust_timer(util_timer* timer); /* 当某个定时器任务发生变化时，调整对应的定时器在链表中的位置 */
    void del_timer(util_timer *timer);    /* 将指定的定时器timer从链表中删除 */
    void tick(); /* SIGALRM信号每次触发就是其信号处理函数（若使用统一事件源，则是主函数）中执行一次tick函数，处理链表上的到期任务 */
private:
    void add_timer(util_timer* timer, util_timer* lst_head); /* 将目标定时器timer添加到lst_head之后的部分链表中 */
    /* 这是一个重载的辅助函数，被公有的add_timer函数和adjust_timer函数调 */
private:
    util_timer* head; /* 头节点 */
    util_timer* tail; /* 尾节点 */
};

class Utils {
public:
    Utils() {}
    ~Utils() {}
    void setnonblocking(int fd); // 设置文件描述符非阻塞
    void addfd(int epollfd, int fd, bool one_shot); // 向epoll中添加需要监听的文件描述符
    void removefd(int epollfd, int fd); // 将文件描述符从epoll中删除
    void modfd(int epollfd, int fd, int ev); // 修改epoll中的文件描述符
    void addsig(int sig, void( handler )(int)); // 设置信号处理函数
    static void sig_handler(int sig);
public:
    static int * u_pipefd;
    static int u_epollfd;
};

void cb_func(client_data *user_data);

#endif