#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <cstdio>
#include "locker.h"

/* 定义线程池类 */

template<typename T>
class threadpool {
public:
    threadpool(int thread_number = 8, int max_requests = 10000); /* 初始化线程池 */
    ~threadpool(); /* 析构函数 */
    bool append(T* request); /* 添加任务 */
private:
    static void* worker(void *arg); /* 工作线程运行的函数，它不断从工作队列中取出任务并执行 */
    void run(); /* 运行线程池 */
private:
    int m_thread_number; /* 线程的数量 */
    pthread_t * m_threads; /* 线程池数组，大小为m_thread_number */
    int m_max_requests; /* 请求队列中最多允许的，等待处理的请求数量 */
    std::list<T *> m_workqueue; /* 请求队列 */
    locker m_queuelocker; /* 保护请求队列的互斥锁 */
    sem m_queuestat; /* 用于判断是否有任务需要处理 */
    bool m_stop; /* 判断是否结束线程 */
};

template<typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) :
    m_thread_number(thread_number),
    m_threads(NULL),
    m_max_requests(max_requests),
    m_stop(false)
{
    // 判断传入的线程池数量以及最大请求数量是否合法
    if ((thread_number <= 0) || (max_requests <= 0)) {
        throw std::exception();
    }
    // 初始化线程池数组
    m_threads = new pthread_t[m_thread_number];
    // 创建thread_number个线程并设置为线程分离
    for (int i = 0; i < thread_number; ++i) {
        printf("create the %dth thread.\n", i);
        if (pthread_create(m_threads+i, NULL, worker, this) != 0) {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]) != 0) {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool() {
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T * request) {
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) {
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post(); //信号量加一
    return true;
}

template<typename T>
void* threadpool<T>::worker(void * arg) {
    threadpool * pool = (threadpool *) arg;
    pool->run();
    return pool; //这个返回值没什么用
}

template<typename T>
void threadpool<T>::run() {
    while( !m_stop ) {
        m_queuestat.wait(); //若信号量为0则阻塞，表示当前没有任务要处理
        m_queuelocker.lock();
        if (m_workqueue.empty()) {//请求队列为空
            m_queuelocker.unlock();
            continue;
        }
        //请求队列不为空，取出一个客户请求并执行
        T * request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if (!request) continue;
        request->process(); //所有的客户请求都要有process方法
    }
}

#endif