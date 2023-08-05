#ifndef UTILS_H
#define UTILS_H

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
#include "../timer/lst_timer.h"
#include "../http/http_conn.h"

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