# WebServer version3.0

## 运行
```shell
./WebServer [-t thread_numbers] [-p port] [-l log_file_path(should begin with '/')]
```
## 目标：
* 使用Epoll边沿触发的IO多路复用技术，非阻塞IO，使用Reactor模式
* 使用多线程充分利用多核CPU，并使用线程池避免线程频繁创建销毁的开销
* 使用基于小根堆的定时器关闭超时请求
* 主线程只负责accept请求，并以Round Robin的方式分发给其它IO线程(兼计算线程)，锁的争用只会出现在主线程和某一特定线程中。
* 使用eventfd实现了线程的异步唤醒
* 使用生产者消费者模型（双缓冲区技术）实现了简单的异步日志系统
* 为减少内存泄漏的可能，使用智能指针等RAII机制
* 使用状态机解析了HTTP请求,支持管线化
* 支持优雅关闭连接

## 并发模型

MainReactor只有一个，负责响应client的连接请求，并建立连接，它使用一个NIO Selector。

在建立连接后用Round Robin的方式在eventLoopThreadPools中寻找，并分配给某个SubReactor, 因为涉及到跨线程任务分配，需要加锁，这里的锁由某个特定线程中的loop创建，只会被该线程和主线程竞争。

SubReactor可以有一个或者多个，每个subReactor都会在一个独立线程中运行，并且维护一个独立的NIO Selector。

当主线程把新连接分配给了某个SubReactor，该线程此时可能正阻塞在多路选择器(epoll)的等待中，怎么得知新连接的到来呢？

这里使用了eventfd进行异步唤醒（wakeupFd），线程会从epoll_wait中醒来，得到活跃事件，进行处理。

## 参考
- https://github.com/linyacool/WebServer
- [万字长文梳理Muduo库核心代码及优秀编程细节思想剖析](https://blog.csdn.net/Jenny_WJN/article/details/104209062)
- [基于muduo网络库的Webserver](https://zhuanlan.zhihu.com/p/533897842)