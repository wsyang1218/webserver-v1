#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "Channel.h"
#include "EpollPoller.h"
#include "Util.h"
#include "../base/CurrentThread.h"
#include "../base/Logging.h"
#include "../base/Thread.h"

using namespace std;
#include <iostream>
// 每个线程只能有一个EventLoop对象，因此在构造函数中要检查当前线程是否已经创建了其他EventLoop对象，遇到错误就终止程序：
// 1. EventLoop构造函数要记住本对象所属的线程（threadId_）
// 2. 创建了EventLoop对象的线程是IO线程，主要功能是运行时间循环（loop()）
// 3. 每个Loop维护一个Epoll，用于获取监听的事件

class EventLoop {
public:
    typedef function<void()> Functor;
    typedef shared_ptr<Channel> SPChannel;
    EventLoop();
    ~EventLoop();
    void loop();
    void quit();
    void runInLoop(Functor && cb);
    void queueInLoop(Functor && cb);
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
    void assertInLoopThread();

    // 修改poller中监听的文件描述符（channel）的状态
    void shutdown(SPChannel channel) { shutDownWR(channel->getfd()); } // 断开套接字的输出流
    void addToPoller(shared_ptr<Channel> channel, int timeout = 0) { poller_->addfd(channel, timeout); }
    void updatePoller(SPChannel channel, int timeout = 0) { poller_->modfd(channel, timeout); }
    void removeFromPoller(SPChannel channel) { poller_->delfd(channel); }
private:
    void wakeup();
    void handleRead();
    void doPendingFunctors();
    void handleConn();
private:
    bool looping_;
    std::shared_ptr<EpollPoller> poller_;
    int wakeupFd_;
    bool quit_;
    bool eventHandling_; // 是否正在执行event
    mutable MutexLock mutex_;
    std::vector<Functor> pendingFunctors_;
    bool callingPendingFunctors_; // 是否唤醒等待函数？用于帮助完成除了IO任务外的计算任务
    const pid_t threadId_; // 运行该loop的thread的id
    std::shared_ptr<Channel> pwakeupChannel_; // 当前被唤醒的channel
};