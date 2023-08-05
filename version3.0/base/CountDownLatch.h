#pragma once
#include "Condition.h"
#include "MutexLock.h"
#include "noncopyable.h"

// 实现门锁，用于确保Thread中传进去的func真正启动了以后外层的start才返回
class CountDownLatch : noncopyable {
public:
    explicit CountDownLatch(int count)
        : mutex_(), cond_(mutex_), count_(count) {}
    void await() {
        MutexLockGuard lock(mutex_); // 调用RAII锁
        while (count_ > 0) cond_.wait(); // 等待计数器值为0，若不为零则一直阻塞
    }
    void countDown() {
        MutexLockGuard lock(mutex_); // 调用RAII锁
        --count_; // 计数器递减
        if (count_ == 0) cond_.broadcast(); // 若计数器为零则通知所有线程
    }
private:
    mutable MutexLock mutex_;
    Condition cond_;
    int count_;
};