#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <pthread.h>
#include <list>
#include <cstdio>
#include "locker.h"

/* 定义线程池类 
 * 原理: 
 * - 线程池是由服务器预先创建并初始化的一组线程。
 * - 当服务器正式运行后需要处理客户请求时，就从池中取出一个线程即可，处理完一个客户连接后，再把线程放回池中即可。 
 * 变量说明: 
 * 1. 用到了模版定义一个任务T，方便代码复用，可能不同线程需要处理不同任务。
 * 2. 线程池用一个数组m_threads表示，线程的数量为m_thread_number。
 * 3. 服务器需要处理的客户请求被维护在一个请求队列中，用链表list<*T>表示，链表最大长度限制为m_max_requests
 * 4. 由于多个线程都能访问请求队列，因此需要用一个互斥锁m_queuelocker保护请求队列
 * 5. 定义一个信号量 m_queuestat，用于判断是否有任务需要处理。
 * 6. 定义一个布尔类型变量m_stop，用于判断是否结束数量。
 * 方法说明:
 * - 构造函数:
 *   1. 初始化变量，包括初始化线程池数组
 *   2. 创建thread_number个线程并设置为线程分离，之所以要设置为线程分离是位为了方便回收线程。
 * - 析构函数:
 *   1. 释放线程池 m_threads;
 *   2. 将m_stop置为true，通知线程释放
 * - append: 向请求队列添加请求
 *   1. 由于请求队列是多个线程的共享资源，因此在操作前需要加锁(m_queuelocker)
 *   2. 判断当前的请求队列长度是否大于m_max_request，若大于则返回false，否则将用户请求加入请求队列。
 *   3. m_queuestat信号量调用post操作，表示多了一个任务要处理
 * 私有方法说明：
 * - worker: 作为线程函数，线程函数必须要求是静态函数（因为pthread_create函数要求线程满足函数必须
 *   满足格式 void *threadfun(void *args)，而普通的成员函数/虚函数实际上的格式为 
 *   void *className::threadfun(this, void *args），因此不满足要求），为了让静态的worker函数
 *   能够访问类的非静态对象，采用的方法是将 this 置针作为参数传入 worker 函数。
 * - run: 线程池运行
 *   1. 循环等待任务处理，直到m_stop为true；
 *   2. 调用信号量m_queuestat的wait方法：
 *      - 若信号量的值为0，则阻塞，说明当前没有待处理任务
 *      - 若信号量值不为0，先判断请求队列是否为空，若不为空则取出一个任务执行
 *   3. 由于需要用到请求队列，需要在操作请求队列前进行加锁，结束后解锁
 */

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