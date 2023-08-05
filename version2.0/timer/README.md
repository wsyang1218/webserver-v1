# 定时器

## 目标
用于检测非活跃连接，由于内核的文件描述符数量是有限的，一个用户连接会占用一个文件描述符，如果非活跃用户长期占用文件描述符，会导致新用户无法连接进服务器而响应失败。

具体的实现方法是通过定时器定期检测客户连接到活跃状态，当检测到用户连接超时后，则将连接关闭。

## 信号处理
Linux 提供了三种定时方法，分别是：
- socket 选项 SO_RCTTIMEO 和 SO_SNDTIMEO
- SIGALRM 信号
- I/O复用系统调用的超时参数。

本项目采用了 SIGALRM 信号实现定时器功能。通过 `alarm` 函数设置定时时间，当闹钟超时将触发一个 `SIGALRM` 信号，可以利用该信号的信号处理函数来处理定时任务。

因此，首先需要在主线程实现对**信号处理**。

信号是一种异步事件，信号处理函数和程序的主循环是两条不同的执行路线，而为了避免一些竞态条件，在信号处理期间系统不会再次触发它，因此要求信号处理函数能尽可能快的执行完毕。

一种常用的解决方案是：
1. 在信号处理中利用**管道**将信号传递给主循环，往管道的写端写入信号值
2. 主循环通过I/O多路复用来监听管道读端，从管道读端读出信号值，再进行信号处理。

这样一来信号事件就和其他的I/O事件一样被处理，即**统一事件源**。

## 定时器的实现

定时器需要包含两个成员：超时时间、任务回调函数。

利用定时器检测非活跃连接时，回调函数中要做的就是将关闭该连接，并将该连接的文件描述符从 epoll 内核事件表中删掉。

有两种实现方法：基于双向链表、基于优先队列（一开始用的双向链表，后来改成了优先队列）

具体的流程为：
1. 定义双向**匿名管道**，用于接收信号，当有定时事件发生时，会调用定时信号的处理函数，该函数会将接收到的信号写入管道；
2. 为每个 http 分配一个定时器，在创建连接时初始化定时器，并设置超时时间，当该连接有读写事件发生时，则重置定时器，当连接超时后会发送一个 `SIGTERM` 信号到管道。
3. 主线程中将管道的读端加入到 epoll 内核事件表中进行监听，当监听到管道有数据可读时，代表有信号事件发生，若该信号是定时信号，则将 `timeout` 设为 `true`，当处理完其他时间后再对定时事件进行处理（定时任务的优先级比较低，通过标记 `timeout` 实现延后处理定时时间）
4. 处理定时时间实际上就是调用 `tick()` 方法，该方法会查询链表/队列中所有超时的定时器，并调用定时器其回调函数。

### 升序链表
第一版本采用的方法是基于**升序链表**的定时器（`lst_timer.h`），就是用一个双向链表将所有的定时器串联起来，并按照超时时间升序排序。
1. 插入一个定时器：从头节点开始遍历链表，当找到一个超时时间大于目标定时器的超时时间的节点，就将定时器插入到该节点之前（时间复杂度为 `O(n)`）
2. 删除一个定时器：只需要修改该定时器的前驱后继节点即可（时间复杂度为 `O(1)`）
3. 处理链表上的超时事件（`tick`）：获取系统当前时间，从头节点向后遍历，若当前节点的超时时间小于系统当前时间，则定时器超时，调用定时器回调函数，直到遇到第一个超时时间大于系统当前时间的节点，就跳出循环。（`SIGALRM`信号每次触发就执行一次 `tick` 函数）

### 优先队列
由于基于**升序链表**的定时器插入一个定时器所需的时间复杂度太高，考虑用优先队列进行优化。