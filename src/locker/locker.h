#ifndef LOCKER_H
#define LOCKER_H

#include <exception>
#include <pthread.h>
#include <semaphore.h>

/* 互斥锁类 */
class locker {
public:
    locker() {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) {
            throw std::exception();
        }
    }

    ~locker() {
        if (pthread_mutex_destroy(&m_mutex) != 0) {
            throw std::exception();
        }
    }

    bool lock() {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock() {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t * get() {
        return &m_mutex;
    }
private:
    pthread_mutex_t m_mutex;
};

/* 条件变量类 */
class cond {
public:
    /* 初始化条件变量 */
    cond() {
        if (pthread_cond_init(&m_cond, NULL) != 0) {
            throw std::exception();
        }
    }

    /* 释放条件变量 */
    ~cond() {
        if (pthread_cond_destroy(&m_cond) != 0) {
            throw std::exception();
        }
    }

    /* 等待条件变量 */
    bool wait(pthread_mutex_t * mutex) {
        return pthread_cond_wait(&m_cond, mutex) == 0;
    }

    /* 等待特定时间 */
    bool timewait(pthread_mutex_t * mutex, struct timespec t) {
        return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
    }

    /* 唤醒条件变量（唤醒一个或者多个线程） */
    bool signal() {
        return pthread_cond_signal(&m_cond) == 0;
    }

    /* 唤醒所有的线程 */
    bool broadcast() {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};

/* 信号量类 */
class sem {
public:
    /* 初始化信号量 */
    sem() {
        if (sem_init(&m_sem, 0, 0) != 0) {
            throw std::exception();
        }
    }

    /* 初始化信号量并设置初始值*/
    sem(int num) {
        if (sem_init(&m_sem, 0, num) != 0) {
            throw std::exception();
        }
    }

    /* 释放信号量 */
    ~sem() {
        sem_destroy(&m_sem);
    }

    /* P操作*/
    bool wait() {
        return sem_wait(&m_sem) == 0;
    }

    /* V操作 */
    bool post() {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

#endif