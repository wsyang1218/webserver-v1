#include "server.h"

Server::Server( int port ): 
        stop_server(false),
        timeout(false),
        port(port)
{
    users = new http_conn[ MAX_FD ];
    users_timer = new client_data[ MAX_FD ];
}

Server::~Server() {
    close(epollfd);
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete[] users;
    delete[] users_timer;
    delete pool;
}

void Server::init_log(char * log_path, bool is_async) {
    if (is_async == true) {
        logger::getInstance()->init(log_path, 800);
    } else {
        logger::getInstance()->init(log_path, 0);
    }
}

void Server::create_threadpool() {
    // 初始化线程池
    try {
        pool = new threadpool<http_conn>();
    } catch(...) {
        exit(-1);
    }
}

void Server::set_events() {
    //创建socket -> 设置端口复用 -> 绑定socket -> 设置监听
    listenfd = socket(PF_INET, SOCK_STREAM, 0); //创建socket
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)); //设置端口复用（注意端口复用的设置时机要在绑定之前）
    struct sockaddr_in addr; //定义socket地址结构体
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY就是指定地址为0.0.0.0的地址，表示本机所有ip，不管数据从哪个网卡来，只要是发送到了绑定的端口号，该socket都可以接收到
    addr.sin_port = htons(port); //host to net (short)函数将主机字节序转换为网络字节序
    bind(listenfd, (struct sockaddr *) &addr, sizeof(addr)); //绑定套接字
    listen(listenfd, 5); //监听socket，5表示内核监听队列的最大长度，一般都设置成5
    
    // 设置io多路复用
    epollfd = epoll_create(5); //创建epoll内核事件表，返回一个文件描述符
    utils.addfd(epollfd, listenfd, false); // 将监听文件描述符添加到epollfd中
    http_conn::m_epollfd = epollfd; //所有的http连接共享同一个epollfd

    // 初始化用于信号处理的管道
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
    if (ret < 0) {
        LOG_ERROR("build pipefd failure\n");
        exit(-1);
    }
    utils.setnonblocking(pipefd[1]); //设置写端非阻塞
    utils.addfd(epollfd, pipefd[0], false); //将读端文件描述符加入epoll

    // 初始化信号处理
    utils.addsig(SIGPIPE, SIG_IGN); //进程收到SIGPIPE信号的默认行为是终止进程，这里设置成ignore
    utils.addsig(SIGALRM, utils.sig_handler);
    utils.addsig(SIGTERM, utils.sig_handler);
    alarm(TIMESLOT);

    Utils::u_pipefd = pipefd;
    Utils::u_epollfd = epollfd;
    return;
}

void Server::start() {
    // 初始化定时器
    while (!stop_server) {
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if ( ret < 0 && errno != EINTR ) {
            LOG_ERROR("epoll failure\n");
            break;
        }
        for (int i = 0; i < ret; ++i) {
            int sockfd = events[i].data.fd; //获取监听到的文件描述符
            if (sockfd == listenfd) {
                // 处理监听事件，加入新的连接
                deal_listen_event();
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 处理连接异常断开事件
                users[sockfd].close_conn(); // 关闭连接
            }
            else if (events[i].events & EPOLLIN)  {
                // 处理可读事件
                deal_read_event(sockfd);
            }
            else if (events[i].events & EPOLLOUT) {
                // 处理可写事件
                deal_write_event(sockfd);
            }
            else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)) {
                // 处理信号
                deal_sig_event();
            }
        }
        // 处理定时事件
        // 用timeout标记有定时事件需要处理，但是不立即处理，因为定时任务的优先级比较低，需要优先处理其它任务
        if (timeout) {
            timer_lst.tick(); //处理定时器任务实际上就是调用tick信号
            alarm(TIMESLOT); //alarm调用只会引起一次SIGALRM信号，所以要重新定时，以不管出发SIGALRM信号
            timeout = false;
        }
    }
}

void Server::send_error(int fd, const char * info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if (ret < 0) {
        LOG_WARN("Send error to client[%d] error!", fd);
    }
}

void Server::add_client(int fd, sockaddr_in & addr) {
    users[fd].init(fd, addr);//将新的客户数据初始化放到数组中，直接将connfd作为索引
    //设置用户的定时器
    users_timer[fd].sockfd = fd;
    users_timer[fd].address = addr;
    //创建定时器
    util_timer* timer = new util_timer;
    timer->user_data = &users_timer[fd]; //绑定用户数据
    timer->cb_func = cb_func; //设置回调函数
    time_t cur = time(NULL); //获取当前时间
    timer->expire = cur + 3 * TIMESLOT; //设置超时时间
    users_timer[fd].timer = timer;
    timer_lst.add_timer(timer); //将定时器加入到链表
}

void Server::deal_listen_event() {
    struct sockaddr_in client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    int connfd = accept(listenfd, (struct sockaddr*)&client_addr, &client_addrlen); //返回连接的fd
    if (http_conn::m_user_count >= MAX_FD ) { /* 当前连接数量满了 */
        LOG_WARN("clients is full!");
        send_error(connfd, "Server busy!");
        close(connfd);
        return;
    }
    add_client(connfd, client_addr);
}

void Server::deal_read_event(int fd) {
    util_timer* timer = users_timer[fd].timer;
    if (users[fd].read()) { //一次性读出所有数据
        pool->append(users + fd);
        //调整该连接对应的定时器，以延迟该连接被关闭的事件
        if (timer) {
            time_t cur = time(NULL);
            timer->expire = cur + 3 * TIMESLOT;
            LOG_INFO("adjust timer once\n");
            timer_lst.adjust_timer(timer);
        }
    } else { //读失败了或没有读到数据，就关闭连接
        users[fd].close_conn();
        if (timer) {
            timer_lst.del_timer(timer); //移除对应的定时器
        }
    }
}

void Server::deal_write_event(int fd) {
    util_timer* timer = users_timer[fd].timer;
    if (users[fd].write()) {//一次性写完所有数据
        //调整连接对应的定时器，延迟该连接被关闭的事件
        if (timer) {
            time_t cur = time(NULL);
            timer->expire = cur + 3 * TIMESLOT;
            LOG_INFO("adjust timer once\n");
            timer_lst.adjust_timer(timer);
        }
    } else {
        users[fd].close_conn();
        if (timer) {
            timer_lst.del_timer(timer); //移除对应的定时器
        }
    }
}

void Server::deal_sig_event() {
    int sig; 
    char signals[1024];
    int ret = recv(pipefd[0], signals, sizeof(signals), 0); //一次性获取所有信号
    if (ret == -1) {
        LOG_WARN("Get signal data failed.\n");
        return;
    } else if (ret == 0) {
        return;
    } else {
        for (int i = 0; i < ret; ++i) {
            switch (signals[i]) {
            case SIGALRM:
                timeout = true;
                break;
            case SIGTERM:
                stop_server = true;
                break;
            default:
                break;
            }
        }
    }
}
