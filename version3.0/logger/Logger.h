#pragma once
#include <iostream>
#include <memory>
#include <list>
#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>
#include <algorithm>
#include <string>
#include <dirent.h>
#include <sys/stat.h>

#include "BufferLog.h"
#include "..//MutexLock.h"
#include "../locker/Condition.h"
#include "../thread/Thread.h"
#include "../utils/TimeSinceGMT.h"


#define LOG_INFO(...)        Logger::logStream("INFO",	 __FILE__, __LINE__,  __VA_ARGS__)
#define LOG_WARN(...)     Logger::logStream("WARNING", __FILE__, __LINE__,  __VA_ARGS__)
#define LOG_ERROR(...)       Logger::logStream("ERROR",   __FILE__, __LINE__,  __VA_ARGS__)
#define LOG_FATAL(...)       Logger::logStream("FATAL",   __FILE__, __LINE__,  __VA_ARGS__)

//单列模式封装的多缓冲区，异步日志类，用于打印日志
class Logger:noncopyable {
public:
	/* 得到日志类实例 */
	static Logger * getLogger();
	//static std::shared_ptr<Logger> setLogger(size_t bufSize);

	/* 按格式输出日志信息到指定文件 */
	//static void logStream(const char* pszLevel, const char* pszFile, int lineNo, const char* pszFuncSig,const char* pszFmt, ...);
	static void logStream(const char* pszLevel, const char* pszFile, int lineNo, const char* pszFmt, ...);
	/*启动日志类*/
	static bool start(bool allowLog_ = false);
	/*终止日志类*/
	static void stop();
	/*设置日志文件名*/
	static void setLogFileName(const char * fileName);
	/*测试用了多少次io操作*/
	static int ioNumbers;
	/*析构函数*/
	~Logger();
private:
    /*默认构造函数*/
	Logger();
	/* 这里没有使用shared_ptr智能指针管理log类,否者由于使用的是静态变量无法析构 */
	static Logger * myLogger;
	/* 当前缓冲 */
	static std::shared_ptr<BufferLog> curBuf;
	/* list管理备用缓冲 */
	static std::list<std::shared_ptr<BufferLog>> bufList;
	/* 备用缓冲中返回一块可用的缓冲 */
	static std::shared_ptr<BufferLog>& useFul();
	/* 条件变量 */
	static Condition readableBuf;
	/* 互斥锁 */
	static MutexLock mutex;
	static MutexLock mutexForGet;
	/* 后台线程需要处理的BufferLog数目 */
	static int readableNum;
	/* 后台线程 */
	static Thread readThread;
	/* 线程执行函数 */
	static void Threadfunc();
	static void func();
	/*func函数是否执行标志位*/
	static bool isRunningFunc;
	/*Threadfunc函数是否执行标志位*/
	static bool isRunningThreadFunc;
	/*Logger是否已经启动标志位*/
	static bool startd;
	/*允许打印日志标志位*/
	static bool allowLog;
	static void setAllowLog(bool allowLog_ = true);
	/*文件描述符*/
	static int fd;
	/*日志文件名*/
	static std::string logFileName;
	/*当前时间格式化*/
	static int timeFormate();
    static TimeSinceGMT timeNow;
};


