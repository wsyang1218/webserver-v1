#pragma once
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <cstdio>
#include "MutexLock.h"
#include "noncopyable.h"

// 条件变量类
class Condition : noncopyable {
public:
    explicit Condition(MutexLock &mutex) : mutex_(mutex) { 
        pthread_cond_init(&cond_, NULL); 
    }
    ~Condition() { pthread_cond_destroy(&cond_); }
    void wait() { pthread_cond_wait(&cond_, mutex_.get()); }
    bool timewait(int seconds) { 
        struct timespec abstime;
        clock_gettime(CLOCK_REALTIME, &abstime);
        abstime.tv_sec = static_cast<time_t>(seconds);
        return ETIMEDOUT == pthread_cond_timedwait(&cond_, mutex_.get(), &abstime);
    }
    void signal() { pthread_cond_signal(&cond_); } // 唤醒条件变量（唤醒一个或者多个线程）
    void broadcast() { pthread_cond_broadcast(&cond_); } // 唤醒所有的线程
private:    
    pthread_cond_t cond_;
    MutexLock &mutex_;
};