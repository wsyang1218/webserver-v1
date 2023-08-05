#pragma once

#include <vector>
#include <memory>
#include <assert.h>
#include "EventLoopThread.h"
#include "../base/Logging.h"
#include "../base/noncopyable.h"

class EventLoopThreadPool : noncopyable {
public:
    EventLoopThreadPool(EventLoop * baseloop, int numThreads);
    ~EventLoopThreadPool() { LOG << "~EventLoopThreadPool()"; }
    void start();
    EventLoop * getNextLoop();
private:
    EventLoop* baseloop_;
    bool started_;
    int numThreads_;
    int next_;
    std::vector<std::shared_ptr<EventLoopThread>> threads_; // 用数组管理所有的线程的shared_ptr
    std::vector<EventLoop*> loops_; // 数组管理线程对应的eventloop的引用
};