#ifndef HEAPTIMER_H
#define HEAPTIMER_H

#include <functional>
#include <chrono>
#include <queue>

using namespace std;

struct TimerNode {
    int sockfd; // 连接套接字的文件描述符，用于定位用户
    chrono::time_point<chrono::high_resolution_clock>  expire; // 超时时间
    function<void()> cb_func; // 回调函数
    bool operator<(const TimerNode& b) const { return expire > b.expire; }
    // STL的优先队列默认是大顶堆，这里需要实现小顶堆，保证根节点的超时时间是最小的
};

class HeapTimer {
private:
    priority_queue<TimerNode> m_queue; // 定义优先队列用于存储定时器节点

};

#endif