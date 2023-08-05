#include "CurrentThread.h"

namespace CurrentThread {
    __thread int t_cachedTid = 0; // 线程真实的pid(tid)的缓存
                                  // 是为了减少syscall(SYS_gettid)系统调用的次数，提高获取tid的效率
    __thread char t_tidString[32]; // 这是tid的字符串表示形式
    __thread const char* t_threadName = "unknown"; // 每个线程的名称
}