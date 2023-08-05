#pragma once
#include <sys/epoll.h>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

class EventLoop;
class HttpData;

// Channel相当于一个文件描述符的保姆：
// 在TCP网络编程中，想要IO多路复用监听某个文件描述符，就要把这个fd和该fd感兴趣的事件通过epoll_ctl注册到IO多路复用模块（我管它叫事件监听器）上
// 当事件监听器监听到该fd发生了某个事件。事件监听器返回 [发生事件的fd集合] 以及 [每个fd都发生了什么事件]
// Channel类则封装了：
// - 该Channel负责照看的[fd]
// - [fd感兴趣事件] 
// - 事件监听器监听到 [该fd实际发生的事件]
// 同时Channel类还提供了：
// 1. 设置该fd的感兴趣事件(setEvents)，提供获取该fd感兴趣的事件的接口（getEvents）
// 2. 当事件监听器监听到该文件描述符上发生了事件，将文件描述符实际发生的事件写入Channel (setRevents)
// 2. 将该fd及其感兴趣事件注册到事件监听器或从事件监听器上移除
// 3. 保存了该fd的每种事件对应的处理函数

class Channel {
private:
    // 该Channel的文件描述符上各类事件发生时的处理函数
    typedef std::function<void()> CallBack;
    CallBack readHandler_;
    CallBack writeHandler_;
    CallBack errorHandler_;
    CallBack connHandler_;
public:
    Channel(EventLoop *loop) : loop_(loop), fd_(0), events_(0), lastEvents_(0) {}
    Channel(EventLoop *loop, int fd) : loop_(loop), fd_(fd), events_(0), lastEvents_(0) {}
    ~Channel() {}

    int getfd() { return fd_; }
    void setfd(int fd) { fd_ = fd; }
    void setHolder(std::shared_ptr<HttpData> holder) { holder_ = holder; }    
    std::shared_ptr<HttpData> getHolder() {
        std::shared_ptr<HttpData> ret(holder_.lock());
        return ret;
    }
     
    // 向Channel注册各类事件的处理函数
    void setReadHandler(CallBack cb) { readHandler_ = std::move(cb); }
    void setWriteHandler(CallBack cb) { writeHandler_ = std::move(cb); }
    void setErrorHandler(CallBack cb) { errorHandler_ = std::move(cb); }
    void setConnHandler(CallBack cb) { connHandler_ = std::move(cb); }

    // 事件处理
    void handleEvents() {
        events_ = 0;
        if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
            events_ = 0;
            return;
        }
        if (revents_ & EPOLLERR) {
            if (errorHandler_) errorHandler_();
            events_ = 0;
            return;
        }
        if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
            if (readHandler_) readHandler_();
        }
        if (revents_ & EPOLLOUT) {
            if (writeHandler_) writeHandler_();
        }
        if (connHandler_) connHandler_();
    }

    // 设置及获取events、revents、lastEvents
    void setEvents(int ev) { events_ = ev; }
    void setRevents(int ev) { revents_ = ev; }
    int & getEvents() { return events_; }

    bool equalAndUpdateLastEvents() {
        bool ret = (lastEvents_ == events_);
        lastEvents_ = events_;
        return ret;
    }
    int getLastEvents() { return lastEvents_; }


private:
    EventLoop *loop_; // 该fd所属的EventLoop，一个Channel只能属于一个EventLoop
    int fd_; // 这个Channel照看到文件描述符
    int events_; // 这个fd感兴趣的事件类型集合
    int revents_; // 事件监听器实际监听到的该fd发生的事件类型集合
    int lastEvents_; //??
    std::weak_ptr<HttpData> holder_; // 方便找到上层持有该Channel的对象
};
typedef std::shared_ptr<Channel> SPChannel;