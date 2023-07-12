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
#include "code/locker.h"
#include "code/threadpool.h"

#include <iostream>

/* 设置信号处理函数 */
void addsig(int sig, void( handler )(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

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
}