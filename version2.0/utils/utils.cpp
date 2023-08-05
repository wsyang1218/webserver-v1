#include "utils.h"


/* 设置文件描述符非阻塞 */
void Utils::setnonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    old_flag |= O_NONBLOCK;
    fcntl(fd, F_SETFL, old_flag);
}

/* 向epoll中添加需要监听的文件描述符 */
void Utils::addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event); //向epoll中添加事件
    setnonblocking(fd); //设置文件描述符非阻塞
}

/* 将文件描述符从epoll中删除 */
void Utils::removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, 0);
    close(fd);
}

/* 修改epoll中的文件描述符 */
void Utils::modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP; //需要重置EPOLLONESHOT事件，确保下次可读时，EPOLLIN事件能够被触发
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/* 设置信号处理函数 */
void Utils::addsig(int sig, void( handler )(int)) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    sigaction(sig, &sa, NULL);
}

void Utils::sig_handler(int sig) {
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

class Utils;
void cb_func(client_data * user_data) {
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    http_conn::m_user_count--;
}
