#include "lst_timer.h"
#include "../http/http_conn.h"

/* 链表被销毁时，需要删除其中所有的定时器 */
sort_timer_lst::~sort_timer_lst() {
    util_timer* tmp = head;
    while (tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

/* 将目标定时器timer加入到链表中 */
void sort_timer_lst::add_timer(util_timer* timer) {
    if (!timer) return;
    if (!head) {
        head = tail = timer;
        return;
    }
    //若目标定时器的时间小于当前链表中所有定时器的超时时间，则将该定时器插入到链表的头部
    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        return;
    }
    //将timer插入到链表合适的位置
    add_timer(timer, head);
}

/* 当某个定时器任务发生变化时，调整对应的定时器在链表中的位置 */
void sort_timer_lst::adjust_timer(util_timer* timer) {
    if (!timer) return;
    util_timer *tmp = timer->next;
    //若被调整的目标定时器处于链表尾部，或其超时时间小于下一个定时器的超时时间，则不用调整
    if (!tmp || (timer->expire < tmp->expire)) return;
    //若目标定时器是头节点，则将该定时器从链表中取出并重新插入链表
    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        add_timer(timer, head);
    }
    //若目标定时器不是头节点，则将该定时器从链表中取出，插入到原来所处位置之后的部分链表中
    else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

/* 将指定的定时器timer从链表中删除 */
void sort_timer_lst::del_timer(util_timer *timer) {
    if (!timer) return;
    //链表只有一个定时器，且就是目标定时器
    if ((timer == head) && (timer == tail)) {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }
    //目标定时器是头节点
    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    //目标定时器是尾节点
    if (timer == tail) {
        tail = timer->prev;
        tail->next = NULL;
        delete timer;
        return;
    }
    //目标定时器位于链表中间
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
}

/* SIGALRM信号每次触发就是其信号处理函数（若使用统一事件源，则是主函数）中执行一次tick函数，处理链表上的到期任务 */
void sort_timer_lst::tick() {
    if (!head) return;
    time_t cur = time(NULL); //获取系统当前时间
    util_timer* tmp = head;
    printf("timer tick\n");
    //从头节点开始依次处理每个定时器，直到遇到一个尚未到期的定时器
    while (tmp) {
        //每个定时器使用绝对时间作为超时值，通过比较定时器的超时时间和当前时间判断定时器是否到期
        if (cur < tmp->expire) break; //定时器尚未到期
        tmp->cb_func(tmp->user_data); //调用定时器的回调函数，执行定时任务
        head = tmp->next; //执行完毕定时任务之后，将该定时器从链表中删除，并重置链表头节点
        if (head) head->prev = NULL;
        delete tmp;
        tmp = head;
    }
}

/* 将目标定时器timer添加到lst_head之后的部分链表中 */
void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head) {
    util_timer* prev = lst_head;
    util_timer* tmp = prev->next;
    /* 遍历lst_head节点之后的部分链表，直到找到一个超时时间大于目标定时器的超时时间的节点，
    将目标定时器插入到该节点之前 */
    while (tmp) {
        if (timer->expire < tmp->expire) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }
    //若遍历完链表也没有找到插入位置，则将timer添加到链表的尾部，更新tail
    if (!tmp) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}


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