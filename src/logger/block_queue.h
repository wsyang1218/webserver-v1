#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H
#include <sys/time.h>
#include "../locker/locker.h"

/* 利用循环数组实现线程安全的阻塞队列 */
template <typename T>
class block_queue {
public:
    block_queue(int max_size = 1000); /* 构造函数 */
    ~block_queue(); /* 析构函数 */
    void clear(); /* 清空队列 */
    bool full(); /* 判断队列是否为满 */
    bool empty(); /* 判断队列是否为空 */
    bool front(T &value); /* 返回队首元素 */
    bool back(T &value); /* 返回队尾元素 */
    int size(); /* 返回队列大小 */
    int max_size(); /* 返回队列最大长队 */
    bool push(const T &item); /* 往队列中添加元素 */
    bool pop(T &item); /* 弹出一个元素 */
    bool pop(T &item, int ms_timeout); /* 增加了超时处理 */
private:
    locker m_mutex; /* 定义互斥锁 */
    cond m_cond; /* 定义条件变量 */
    T *m_array; /* 数组 */
    int m_size; /* 数组当前大小 */
    int m_max_size; /* 数组的最大长度 */
    int m_front; /* 数组首元素的下标 */
    int m_back; /* 数组末尾元素的下标 */
};

template <typename T> 
block_queue<T>::block_queue(int max_size) {
    if (max_size <= 0) {
        exit(-1);
    }
    //创建循环数组
    m_max_size = max_size;
    m_array = new T[max_size];
    m_size = 0;
    m_front = -1;
    m_back = -1;
}

template <typename T> 
block_queue<T>::~block_queue() {
    m_mutex.lock();
    if (m_array != NULL) delete[] m_array;
    m_mutex.unlock();
}

template <typename T> 
void block_queue<T>::clear() {
    m_mutex.lock();
    m_size = 0;
    m_front = -1;
    m_back = -1;
    m_mutex.unlock();
}

template <typename T> 
bool block_queue<T>::full() {
    m_mutex.lock();
    if (m_size >= m_max_size) {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template <typename T> 
bool block_queue<T>::empty() {
    m_mutex.lock();
    if (0 == m_size) {
        m_mutex.unlock();
        return true;
    }
    m_mutex.unlock();
    return false;
}

template <typename T> 
bool block_queue<T>::front(T &value) {
    m_mutex.lock();
    if (0 == m_size) {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_front];
    m_mutex.unlock();
    return true;
}

template <typename T> 
bool block_queue<T>::back(T &value) {
    m_mutex.lock();
    if (0 == m_size) {
        m_mutex.unlock();
        return false;
    }
    value = m_array[m_back];
    m_mutex.unlock();
    return true;
}

template <typename T> 
int block_queue<T>::size() {
    int tmp = 0;
    m_mutex.lock();
    tmp = m_size;
    m_mutex.unlock();
    return tmp;
}

template <typename T> 
int block_queue<T>::max_size() {
    int tmp = 0;
    m_mutex.lock();
    tmp = m_max_size;
    m_mutex.unlock();
    return tmp;
}

template <typename T> 
bool block_queue<T>::push(const T &item) {
    //向队列中添加一个元素，相当于生产者生产了一个元素
    m_mutex.lock();
    if (m_size >= m_max_size) {
        m_cond.broadcast();
        m_mutex.unlock();
        return false;
    }
    m_back = (m_back + 1) % m_max_size;
    m_array[m_back] = item;
    m_size++;
    m_cond.broadcast();//唤醒所有线程
    m_mutex.unlock();
}

template <typename T>
bool block_queue<T>::pop(T &item) {
    m_mutex.lock();
    //若当前队列没有元素，则等待条件变量
    while (m_size <= 0) {
        //重新抢到了互斥锁，pthraed_cond_wait返回0，若队列中有元素
        //即 m_size > 0，则跳出while循环，继续向下执行
        if (!m_cond.wait(m_mutex.get())) {
            //pthread_cond_wait返回值不为0，说明出错了，则直接解锁并返回false
            m_mutex.unlock();
            return false;
        }
    }
    m_front = (m_front + 1) % m_max_size; //取出队首元素
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}

template <typename T>
bool block_queue<T>::pop(T &item, int ms_timeout) {
    //在 pthread_cond_wait 基础上增加了等待的时间，在指定时间内抢到互斥锁即可
    struct timespec t = {0, 0};
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    m_mutex.lock();
    if (m_size <= 0) {
        t.tv_sec = now.tv_sec + ms_timeout / 1000;
        t.tv_nsec = (ms_timeout % 1000) * 1000;
        if (!m_cond.timewait(m_mutex.get(), t)) {
            m_mutex.unlock();
            return false;
        }
    }
    if (m_size <= 0) {
        m_mutex.unlock();
        return false;
    }
    m_front = (m_front + 1) % m_max_size;
    item = m_array[m_front];
    m_size--;
    m_mutex.unlock();
    return true;
}


#endif