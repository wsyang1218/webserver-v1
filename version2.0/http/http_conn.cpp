#include "http_conn.h"
#include "../logger/logger.h"

//定义HTTP响应的一些状态信息的描述
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request had had syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";
/* 定义HTTP连接任务类 */
const char* doc_root = "/root/my_webserver/resources"; //网站根目录

/* 初始化静态变量 */
int http_conn::m_epollfd = -1;
int http_conn::m_user_count = 0;

/* 设置文件描述符非阻塞 */
void setnonblocking(int fd) {
    int old_flag = fcntl(fd, F_GETFL);
    old_flag |= O_NONBLOCK;
    fcntl(fd, F_SETFL, old_flag);
}

/* 向epoll中添加需要监听的文件描述符 */
void addfd(int epollfd, int fd, bool one_shot) {
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event); //向epoll中添加事件
    setnonblocking(fd); //设置文件描述符非阻塞
}

/* 将文件描述符从epoll中删除 */
void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, 0);
    close(fd);
}

/* 修改epoll中的文件描述符 */
void modfd(int epollfd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP; //需要重置EPOLLONESHOT事件，确保下次可读时，EPOLLIN事件能够被触发
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

/* 初始化新接收的连接 */
void http_conn::init(int sockfd, const sockaddr_in & addr) {
    m_sockfd = sockfd; //方便后续针对该sockfd进行读写操作
    m_addr = addr; //设置地址
    //设置端口复用
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    //添加到epoll对象中
    addfd(m_epollfd, m_sockfd, true); //需要设置为EPOLLONESHOT事件
    ++m_user_count; //总用户数加一
    init(); //初始化一些信息
}

/* 初始化其他信息 */
void http_conn::init() {
    m_check_state = CHECK_STATE_REQUESTLINE; //初始化状态为解析请求首行
    m_checked_idx = 0; //当前正在解析的字符在缓冲区的位置初始化为0
    m_write_idx = 0; //初始化当前写缓冲区下标位置为0
    m_start_line = 0 ; //当前正在解析的行的起始位置初始化为0
    m_read_idx = 0; //当前读取到的位置初始化为0
    m_method = GET; //初始化请求方法
    m_url = 0;      //初始化url
    m_version = 0;  //初始化http协议版本
    m_host = 0; //初是阿虎主机名
    m_linger = false; //默认http请求不保持连接
    m_content_length = 0; //初始化请求体长度为0
    
    bytes_to_send = 0;
    bytes_have_send = 0;

    bzero(m_read_buffer, READ_BUFFER_SIZE); //清空读缓冲区
    bzero(m_write_buffer, WRITE_BUFFER_SIZE); //清空写缓冲区
    bzero(m_real_file, FILENAME_LEN); //初始化文件地址
}

/* 关闭一个连接 */
void http_conn::close_conn() {
    if (m_sockfd != -1) {
        removefd(m_epollfd, m_sockfd); //将该连接的文件描述符从epoll内核事件表中删除
        m_sockfd = -1; //置为-1后，这个sockfd就没用了
        --m_user_count; //关闭一个连接，客户总数量减少1
    }
}

/* 一次性非阻塞的读出所有数据 */
bool http_conn::read() {
    if (m_read_idx >= READ_BUFFER_SIZE) { //读缓冲区已满
        return false;
    }
    int bytes_read = 0;
    while (true) {
        bytes_read = recv(m_sockfd, m_read_buffer + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) 
                break; //没有数据了
            return false;
        } else if (bytes_read == 0) {// 对方关闭连接了
            return false;
        }
        m_read_idx += bytes_read; //更新索引
    }
    return true;
}

/* 从状态机，用于解析一行，判断依据是\r\n */ 
http_conn::LINE_STATUS http_conn::parse_line() {
    char temp;
    for ( ; m_checked_idx < m_read_idx; ++m_checked_idx ) {
        temp = m_read_buffer[ m_checked_idx ];
        if ( temp == '\r' ) {
            if ( ( m_checked_idx + 1 ) == m_read_idx ) {
                return LINE_OPEN;
            } else if ( m_read_buffer[ m_checked_idx + 1 ] == '\n' ) {
                m_read_buffer[ m_checked_idx++ ] = '\0';
                m_read_buffer[ m_checked_idx++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if( temp == '\n' )  {
            if( ( m_checked_idx > 1) && ( m_read_buffer[ m_checked_idx - 1 ] == '\r' ) ) {
                m_read_buffer[ m_checked_idx-1 ] = '\0';
                m_read_buffer[ m_checked_idx++ ] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

/* 解析请求首行，获取请求方法、目标url、http版本 */
http_conn::HTTP_CODE http_conn::parse_request_line(char * text) {
    //GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t");
    //GET\0/index.html HTTP/1.1
    *m_url++ = '\0';
    char * method = text;
    if (strcasecmp(method, "GET") == 0) {
        m_method = GET; //暂时只支持GET方法
    } else {
        return BAD_REQUEST;
    }
    // /index.html HTTP/1.1
    m_version = strpbrk(m_url, " \t");
    if (!m_version) {
        return BAD_REQUEST;
    }
    // /index.html\0HTTP/1.1
    *m_version++ = '\0';
    if (strcasecmp(m_version, "HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }
    // http://192.168.1.1:10000/index.html
    if (strncasecmp(m_url, "http://", 7) == 0) {
        m_url += 7; //192.168.1.1:10000/index.html
        m_url = strchr(m_url, '/'); // /index.html
    }
    if (!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER; //更新主状态机的状态，变为检查请求头

    return NO_REQUEST;
} 

/* 解析请求头 */
http_conn::HTTP_CODE http_conn::parse_headers(char * text){
    if (text[0] == '\0') {//遇到空行，说明头部字段解析完毕
        if (m_content_length != 0) { //若http请求有请求体，还需要读取 m_content_length 字节的消息体
            m_check_state = CHECK_STATE_CONTENT;//更新主状态机等状态
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        //处理Connection头部字段
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            m_linger = true;
        }
    } else if (strncasecmp(text, "Content-Length:", 15) == 0) {
        //处理Content-Length头部字段
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        //处理Host头部字段
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    } else {
        LOG_WARN("oop! unknown header %s\n", text);
    }
    return NO_REQUEST;
}

/* 处理请求的具体内容， */
http_conn::HTTP_CODE http_conn::do_request() {
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    //获取m_read_file文件的相关状态信息，失败返回-1，成功返回0
    if (stat(m_real_file, &m_file_stat) < 0) {
        return NO_REQUEST;
    }
    if (!(m_file_stat.st_mode & S_IROTH)) {//判断访问权限
        return FORBIDDEN_REQUEST;
    }
    if (S_ISDIR(m_file_stat.st_mode)) { //判断是否为目录
        return BAD_REQUEST;
    }
    int fd = open(m_real_file, O_RDONLY); //以只读的方式打开文件
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

/* 解析请求体*/
http_conn::HTTP_CODE http_conn::parse_content(char * text) {}

/* 主状态机，用于解析http请求 */
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char * text = 0; //要获取的一行数据
    while ( (m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK)  //解析到了请求体，也是完整的数据
            || (line_status = parse_line()) == LINE_OK) { //解析到了一行完整的数据
        text = get_line(); //获取一行数据
        m_start_line = m_checked_idx;
        LOG_INFO("Got 1 http line : %s\n", text);
        switch (m_check_state) {
        case CHECK_STATE_REQUESTLINE: {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST) return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER: {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST) return BAD_REQUEST;
            else if (ret == GET_REQUEST) {
                return do_request(); //解析具体的请求信息
            }
            break;
        }
        case CHECK_STATE_CONTENT: {
            ret = parse_content(text);
            if (ret == GET_REQUEST) {
                return do_request();
            }
            line_status = LINE_OPEN;
            break;
        }
        default: return INTERNAL_ERROR;
        }
    }

    return NO_REQUEST;
} 

/* 释放内存映射区 */
void http_conn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

/* 写HTTP响应，一次性非阻塞的写完所有数据 */
bool http_conn::write() {
    if (bytes_to_send == 0) {//要发送的数据为0，则这次的响应结束
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    int tmp = 0;
    while(true) {
        tmp = writev(m_sockfd, m_iv, m_iv_count); //分散写
        if (tmp <= -1) {
            if (errno == EAGAIN) {
                //TCP写缓冲区没有空间，则等待下一次的EPOLLOUT事件
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            //若为其他错误，则释放内存映射区并返回false
            unmap();
            return false;
        }

        bytes_have_send += tmp;
        bytes_to_send -= tmp;

        if (bytes_have_send >= m_iv[0].iov_len) { //第一块内存块已经写入完毕
            m_iv[0].iov_len = 0; //将第一块内存块长度置为0
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        } else {
            m_iv[0].iov_base = m_write_buffer + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - tmp;
        }

        if (bytes_to_send <= 0) { //没有数据要发送了
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);
            if (m_linger) { //保持连接
                init();
                return true;
            } else {
                return false;
            }
        }

    }
    return true;
}

/* 往写缓冲区写入带发送数据 */
bool http_conn::add_response(const char* format, ...) {
    if (m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buffer + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)) {
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

/* 添加响应状态行 */
bool http_conn::add_status_line(int status, const char* title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

/* 添加响应头部 */
bool http_conn::add_headers(int content_len) {
    add_content_length(content_len);
    add_content_type();
    add_linger();
    add_blank_line();
}

/* 添加Content-Length头部信息 */
bool http_conn::add_content_length(int content_len) {
    return add_response("Content-Length: %d\r\n", content_len);
}

/* 添加Content-Type头 */
bool http_conn::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}

/* 添加Connection头 */
bool http_conn::add_linger() {
    return add_response("Connection:%s\r\n", m_linger ? "keep=alive" : "close");
}

/* 添加空行 */
bool http_conn::add_blank_line() {
    return add_response("%s", "\r\n");
}

/* 添加响应体 */
bool http_conn::add_content(const char* content) {
    return add_response("%s", content);
}

/* 根据服务器处理HTTP请求的结果，决定返回给客户端的内容 */
bool http_conn::process_write(HTTP_CODE ret) {
    switch (ret) {
    case NO_REQUEST: //请求不完整，需要继续读取客户数据
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form)) {
            return false;
        }
        break;
    case GET_REQUEST: //表示获得了一个完整的客户请求
        add_status_line(200, ok_200_title);
        add_headers(0);
        break;
    case BAD_REQUEST: //表示客户请求语法错误
        add_status_line(400, error_400_title);
        add_headers(strlen(error_400_form));
        if (!add_content(error_400_form)) {
            return false;
        }
        break;
    case NO_RESOURCE: //表示服务器没有资源
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form)) {
            return false;
        }
        break;
    case FORBIDDEN_REQUEST: //表示客户对资源没有访问权限
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form)) {
            return false;
        }
        break;
    case FILE_REQUEST: //文件请求，获取文件成功
        add_status_line(200, ok_200_title);
        add_headers(m_file_stat.st_size);
        m_iv[0].iov_base = m_write_buffer;
        m_iv[0].iov_len = m_write_idx;
        m_iv[1].iov_base = m_file_address;
        m_iv[1].iov_len = m_file_stat.st_size;
        m_iv_count = 2;
        bytes_to_send = m_write_idx + m_file_stat.st_size;
        return true;
        break;
    case INTERNAL_ERROR: //服务器内部错误
        add_status_line(500, error_500_form);
        add_headers(strlen(error_500_form));
        add_headers(strlen(error_500_form));
        if (!add_content(error_500_form)) {
            return false;
        }
        break;
    case CLOSED_CONNECTION: //表示客户端已经关闭连接了
        break;
    default:
        break;
    }
    m_iv[0].iov_base = m_write_buffer;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

/* 由线程池的工作线程调用，是处理HTTP请求的入口函数 */
void http_conn::process() {
    //解析http请求
    HTTP_CODE read_ret = process_read(); //解析http请求
    if (read_ret == NO_REQUEST) { //请求不完整
        modfd(m_epollfd, m_sockfd, EPOLLIN); //继续获取用户数据
        return;
    }
    //生成响应
    bool write_ret = process_write( read_ret );
    if (!write_ret) {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}

