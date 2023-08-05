#pragma once

#include <sys/epoll.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <sys/epoll.h>
#include "Channel.h"
#include "EventLoop.h"
#include "HttpData.h"
#include "Timer.h"

// 实现一个事件监听器：
// 功能：1. 负责监听文件描述符事件是否触发；2. 返回发生事件的文件描述符的具体事件
// 说明：在multi-reactor模型中，有多少个reactor就有多少个poller，外界通过调用poll方法开启监听

class EpollPoller : noncopyable {
public:
    EpollPoller();
    ~EpollPoller();

    // 实现增加、修改、删除文件描述符
    // 一个Epoll负责监听多个文件描述符，这里的文件描述符都是由Channel管理的
    // 因此可以通过指向Channel的shared_ptr实现epoll中对应文件描述符监听事件状态的修改
    void addfd(SPChannel request, int timeout);
    void modfd(SPChannel request, int timeout);
    void delfd(SPChannel request);
    int getEpollfd() { return epollfd_; }

    // ************* 重要 ***************
    // poll方法是Poller的核心方法，用于获取内核事件表中最新的事件
    // 返回需要处理的活跃Channel列表（仍然通过智能指针安全返回）
    std::vector<std::shared_ptr<Channel>> poll(); // 开启IO复用

    // 定时器相关
    void addTimer(std::shared_ptr<Channel> req, int timeout);
    void handleExpired() { timerManager_.handleExpiredEvent(); } 
private:
    static const int MAXFDS = 100000;
    int epollfd_; // 通过epoll_create方法返回的epoll句柄
    std::vector<epoll_event> events_; // 内核事件表
    std::shared_ptr<Channel> fd2chan_[MAXFDS];
    std::shared_ptr<HttpData> fd2http_[MAXFDS];

    TimerManager timerManager_; //定时器
};