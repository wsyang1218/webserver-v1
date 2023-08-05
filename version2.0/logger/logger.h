#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <string.h>
#include <string>
#include <time.h>
#include "block_queue.h"
#include "../locker/locker.h"

using namespace std;
class logger {
public:
    /* 提供公有方法用于获取唯一的实例 */
    static logger * getInstance() {
        static logger instance;
        return &instance;
    }
    /* 异步写入日志进程的回调函数 */
    static void *flush_log_thread(void *args) {
        logger::getInstance()->async_write_log();
    }
    /* 初始化，包括初始化参数、日志缓冲区、新建第一个日志文件等 */
    bool init(const char *file_name, int max_queue_size = 0, 
              int split_lines = 5000000,int log_buf_size = 8192);
    
    void write_log(int level, const char *format, ...);
    void flush(void);
private:
    logger();
    ~logger();

    /* 异步写入日志 */
    void *async_write_log() {
        string sinlge_log;
        //从阻塞队列中取出一个日志string，写入文件
        while (m_log_queue->pop(sinlge_log)) {
            m_mutex.lock();
            fputs(sinlge_log.c_str(), m_fp);
            m_mutex.unlock();
        }
    }
private:
    char dir_name[128]; /* 日志文件路经名 */
    char log_name[128]; /* log文件名 */
    int m_split_lines; /* 日志最大行数 */
    int m_log_buf_size; /* 日志缓冲区大小 */
    long long m_count; /* 日志行数记录 */
    int m_today; /* 按天分类 */
    FILE *m_fp; /* 打开log的文件指针 */
    char* m_buf;
    block_queue<string> *m_log_queue; /* 用于异步写入日志的阻塞队列 */
    bool m_is_async; /* 1-异步写入，0-同步写入 */
    locker m_mutex; /* 用于线程同步，保护临界区资源 */
};

#define LOG_DEBUG(format, ...) logger::getInstance()->write_log(0, format, ##__VA_ARGS__); logger::getInstance()->flush();
#define LOG_INFO(format, ...)  logger::getInstance()->write_log(1, format, ##__VA_ARGS__); logger::getInstance()->flush();
#define LOG_WARN(format, ...)  logger::getInstance()->write_log(2, format, ##__VA_ARGS__); logger::getInstance()->flush();
#define LOG_ERROR(format, ...) logger::getInstance()->write_log(3, format, ##__VA_ARGS__); logger::getInstance()->flush();

#endif