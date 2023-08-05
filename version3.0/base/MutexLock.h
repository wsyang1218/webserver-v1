#pragma once
#include <pthread.h>
#include <semaphore.h>
#include "noncopyable.h"

// 封装临界区，用RAII手法封装互斥起的创建和销毁
class MutexLock: noncopyable {
public:
    MutexLock() { pthread_mutex_init(&mutex, NULL); }
    ~MutexLock() {
        pthread_mutex_lock(&mutex);
        pthread_mutex_destroy(&mutex);
    }
    void lock() { pthread_mutex_lock(&mutex); }
    void unlock() { pthread_mutex_unlock(&mutex); }
    pthread_mutex_t *get() { return &mutex; }
private:
    pthread_mutex_t mutex;
};

// 封装临界区的进入和退出，即加锁和解锁
class MutexLockGuard : noncopyable {
public:
    explicit MutexLockGuard(MutexLock &mutex) : mutex_(mutex) { mutex_.lock(); }
    ~MutexLockGuard() { mutex_.unlock(); }
private:
    MutexLock &mutex_;
};
