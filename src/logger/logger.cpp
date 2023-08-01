#include "logger.h"

logger::logger() {
    m_count = 0;
    m_is_async = false;
}

logger::~logger() {
    if (m_fp != NULL) {
        fclose(m_fp); //关闭日志
    }
}

bool logger::init(const char *file_name, int max_queue_size, 
                  int split_lines, int log_buf_size)
{
    //若设置了max_queue_size，则设置为异步
    if (max_queue_size >= 1) {
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        //创建线程用于异步写入日志，flush_log_thread是回调函数
        pthread_t tid;
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }
    m_log_buf_size = log_buf_size; //日志缓冲区大小
    m_split_lines = split_lines; //单个日志文件的最大行数

    //初始化日志缓冲区
    m_buf = new char[log_buf_size]; 
    memset(m_buf, '\0', log_buf_size);

    //获取当前系统时间，用于给日志文件命名
    time_t t = time(NULL);
    struct tm *sys_time = localtime(&t);
    struct tm my_tm = *sys_time;

    const char *p = strrchr(file_name, '/'); //获取文件名
    char log_full_name[256] = {0}; //用于存储完整的日志文件名

    if (p == NULL) {
        snprintf(log_full_name, 355, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, 
            my_tm.tm_mon + 1, my_tm.tm_mday, file_name);
    } else {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p - file_name + 1);
        snprintf(log_full_name, 355, "%d_%02d_%02d_%s", my_tm.tm_year + 1900, 
            my_tm.tm_mon + 1, my_tm.tm_mday, log_name);
    }
    
    m_today = my_tm.tm_mday;
    m_fp = fopen(log_full_name, "a"); //以追加的方式打开日志文件
    if (m_fp == NULL) return false;
    return true;
}

void logger::write_log(int level, const char * format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch (level) {
    case 0:
        strcpy(s, "[debug]:");
        break;
    case 1:
        strcpy(s, "[info]:");
        break;
    case 2:
        strcpy(s, "[warn]:");
        break;
    case 3:
        strcpy(s, "[error]:");
        break;
    default:
        strcpy(s, "[info]:");
        break;
    }

    m_mutex.lock();

    ++m_count; //更新当前行数

    //在写入日志之前，首先判断当前day是否为创建日志的时间，或写入的日志行数是最大行的倍数
    //m_split_lines为最大行数
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0) {
        char new_log[256] = {0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};
        snprintf(tail, 16, "%d_%02d_%02d_", my_tm.tm_year + 1900, my_tm.tm_mon + 1,
            my_tm.tm_mday);
        //当前day不是创建日志的时间，则创建新的log，更新创建m_today和m_count
        if (m_today != my_tm.tm_mday) { 
            snprintf(new_log, 355, "%s%s%s", dir_name, tail, log_name);
            m_today = my_tm.tm_mday;
            m_count = 0;
        } else {
            //若行数超过了最大行数限制，在当前日志的末尾加count/max_lines为后缀创建新log
            snprintf(new_log, 355, "%s%s%s.%lld", dir_name, log_name, m_count / m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }
    
    m_mutex.unlock();
    
    va_list valst; //可变参数变量
    va_start(valst, format); //将传入的format参数赋值给valst，便于格式化输出

    string log_str;
    m_mutex.lock();
    //写入内容的格式：时间 + 内容
    //时间格式化，snprintf成功返回写入字符的总数，其中不包括结尾的null字符
    //例如：20240723 16:37:48.134314 [debug]:
    int n = snprintf(m_buf, 48, "%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
                     my_tm.tm_year + 1900, my_tm.tm_mon + 1, my_tm.tm_mday,
                     my_tm.tm_hour, my_tm.tm_min, my_tm.tm_sec, now.tv_usec, s);
    //内容格式化，vsnprintf用于向字符串数组缓冲区打印格式化字符串
    //第一个参数为缓冲区的起始地址，已经写入了n个字符，此时的起始位置应该为m_buf + n
    //第二个参数为最多能写入的字符总数，m_log_buf_size为缓冲区大小，减去已经写入的n个字符以及1个结束字符
    //format为将要被写入缓冲区的字符串，valst为从format中提取出来的可变参数列表，用于格式化输出
    int m = vsnprintf(m_buf + n, m_log_buf_size - n - 1, format, valst);
    //添加换行符和结束符
    m_buf[n + m] = '\n';
    m_buf[n + m + 1] = '\0';
    
    log_str = m_buf;
    
    m_mutex.unlock();
    
    //m_is_async为true表示异步写入，默认同步
    //若异步，则将日志信息加入到阻塞队列，同步则加锁向文件中写入
    if (m_is_async && !m_log_queue->full()) {
        m_log_queue->push(log_str);
    } else {
        m_mutex.lock();
        fputs(log_str.c_str(), m_fp);
        m_mutex.unlock();
    }
    va_end(valst);
}

void logger::flush(void) {
    m_mutex.lock();
    fflush(m_fp);//强制刷新写入流缓冲区
    m_mutex.unlock();
}