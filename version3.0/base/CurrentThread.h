#pragma once
#include<unistd.h>
#include<sys/syscall.h>
#include<assert.h>
#include<stdio.h>

namespace CurrentThread {
    // 线程局部变量（__thread修饰的变量是线程局部存储的）
    extern __thread int t_cachedTid;
    extern __thread char t_tidString[32];
    extern __thread const char* t_threadName;

    // 缓存线程id
    inline int tid() {
        if (t_cachedTid == 0) { // 该tid没有被缓存过
            cacheTid(); // 缓存tid
        }
        return t_cachedTid;
    }
    inline void cacheTid() {
        if (t_cachedTid == 0) {
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid)); // 获取线程唯一id
            snprintf(t_tidString, sizeof t_tidString, "%d ", t_cachedTid); // 存储id的string格式，用于打印
        }
    }

    // 用于logging
    inline const char* tidString() { return t_tidString; }
    inline const char* name() { return t_threadName; }
    
    // 判断是否是主线程
    inline bool isMainThread() { return tid() == ::getpid(); }
};