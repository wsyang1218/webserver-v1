#include "EpollPoller.h"
#include <assert.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <deque>
#include <queue>
#include <arpa/inet.h>
#include "../base/Logging.h"

using namespace std;

const int EVENTSNUM = 4096; // 可以监听的事件总数
const int EPOLLWAIT_TIME = 10000; 

typedef shared_ptr<Channel> SPChannel;

EpollPoller::EpollPoller() : epollfd_(epoll_create1(EPOLL_CLOEXEC)), events_(EVENTSNUM) {
  assert(epollfd_ > 0);
}
EpollPoller::~EpollPoller() {}

// 注册新的文件描述符
void EpollPoller::addfd(SPChannel request, int timeout) {
    int fd = request->getfd(); // 获取Channel的文件描述符
    // 设置超时时间
    if (timeout > 0) {
        addTimer(request, timeout);
    fd2http_[fd] = request->getHolder();
    }
    struct epoll_event event;
    event.data.fd = fd;
    event.events = request->getEvents(); // 获取Channel注册的事件
    // Channel的注册发生在主线程上

    request->equalAndUpdateLastEvents(); // 比较上次注册的事件和这次时候相同，更新lastEvents
    fd2chan_[fd] = request; // 将Channel添加到该Poller管理的Channel列表中
    if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &event) < 0) { // 更新内核事件表
        perror("epoll_add error"); // 若失败则打印失败消息，并重置管理的Channel列表
        fd2chan_[fd].reset();
    }
}

// 修改文件描述符状态
void EpollPoller::modfd(SPChannel request, int timeout) {
    if (timeout > 0) addTimer(request, timeout); // 若timeout不为零，则添加timer
    int fd = request->getfd();
    if (!request->equalAndUpdateLastEvents()) { // 新的events和旧的events不同
        struct epoll_event event;
        event.data.fd = fd;
        event.events = request->getEvents();
        if (epoll_ctl(epollfd_, EPOLL_CTL_MOD, fd, &event) < 0) {
            perror("epoll_mod error");
            fd2chan_[fd].reset();
        }
    }
}

// 删除文件描述符
void EpollPoller::delfd(SPChannel req) {
    int fd = req->getfd();
    struct epoll_event event;
    event.data.fd = fd;
    event.events = req->getLastEvents();
    if (epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &event) < 0) {
        perror("epoll_del error");
    }
    fd2chan_[fd].reset();
    fd2http_[fd].reset();
}


// 返回活跃事件的channel智能指针列表
std::vector<std::shared_ptr<Channel>> EpollPoller::poll() {
    while (true) {
        int event_count = epoll_wait(epollfd_, &*events_.begin(), events_.size(), EPOLLWAIT_TIME);
        if (event_count < 0) perror("epoll wait error");
        std::vector<SPChannel> req_data;
        for (int i = 0; i < event_count; ++i) {
            int fd = events_[i].data.fd; // 获取有事件产生的描述符
            SPChannel cur_req = fd2chan_[fd]; // 获取该fd的保姆channel
            if (cur_req) {
                cur_req->setRevents(events_[i].events); // Revents就是实际发生的事件
                cur_req->setEvents(0); // 这里把events置为零是为了重新设置监听事件
                req_data.push_back(cur_req);
            } else {
                LOG << "SP cur_req is invalid";
            }
        }
        if (req_data.size() > 0) return req_data; // 若活跃事件列表不为空则返回，否则继续循环等待
    }
}

void EpollPoller::addTimer(std::shared_ptr<Channel> req, int timeout) {
    shared_ptr<HttpData> t = req->getHolder();
    if (t) timerManager_.addTimer(t, timeout); // 每个Http连接都绑定一个Timer
    else LOG << "timer add fail.";
}