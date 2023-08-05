#include "EventLoop.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <iostream>
#include "Util.h"
#include "base/Logging.h"

using namespace std;

// __thread是GCC内置的线程局部存储设施，存取效率可以和全局变量相比
// __thread变量每一个线程有一份独立实体，各个线程的值互不干扰
__thread EventLoop* t_loopInThisThread = 0; // 存储当前thread运行的EventLoop的地址，保证one loop per thread

int createEventfd() {
    int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) {
        LOG << "Failed in eventfd";
        abort();
    }
    return fd;
}

EventLoop::EventLoop() :
    looping_(false),
    poller_(new EpollPoller()),
    wakeupFd_(createEventfd()),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    threadId_(CurrentThread::tid()),
    pwakeupChannel_(new Channel(this, wakeupFd_)) {
    // 保证one loop per thread
    if (t_loopInThisThread) {
        LOG << "Another EventLoop " << t_loopInThisThread 
            << " exists int this thread " << threadId_;
    } else {
        t_loopInThisThread = this;
    }
    pwakeupChannel_->setEvents(EPOLLIN | EPOLLET); // 设置监听读事件，边沿触发模式
    pwakeupChannel_->setReadHandler(bind(&EventLoop::handleRead, this));
    pwakeupChannel_->setConnHandler(bind(&EventLoop::handleConn, this));
    poller_->addfd(pwakeupChannel_, 0);
}

EventLoop::~EventLoop() {
    close(wakeupFd_);
    t_loopInThisThread = NULL;
}

void EventLoop::handleConn() {
    updatePoller(pwakeupChannel_);
}

void EventLoop::runInLoop(Functor&& cb) {
    if (isInLoopThread()) cb();else
    queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(Functor&& cb) {
    {
        MutexLockGuard lock(mutex_);
        pendingFunctors_.emplace_back(std::move(cb));
    }

    if (!isInLoopThread() || callingPendingFunctors_) wakeup();
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = writen(wakeupFd_, (char*)(&one), sizeof(one));
    if (n != sizeof one) {
        LOG << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
    }
}

void EventLoop::loop() {
    assert(!looping_); // 验证当前是否处于looping状态
    assert(isInLoopThread()); // 验证是否运行在正确的线程上
    looping_ = true;
    quit_ = false;
    LOG << "EventLoop " << this << " start looping";
    std::vector<SPChannel> ret;
    while (!quit_) {
        ret.clear();
        ret = poller_->poll(); // 返回活跃用户列表
        eventHandling_ = true;
        for (auto& it : ret) it->handleEvents(); // 每个channel轮流执行任务
        eventHandling_ = false;
        doPendingFunctors();
        poller_->handleExpired(); // 最后再处理超时时间
    }
    looping_ = false;
}

void EventLoop::doPendingFunctors() {
    vector<Functor> functors;
    callingPendingFunctors_ = true;

    {
        MutexLockGuard lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for (size_t i = 0; i < functors.size(); ++i) functors[i]();
    callingPendingFunctors_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    if (!isInLoopThread()) {
        wakeup();
    }
}