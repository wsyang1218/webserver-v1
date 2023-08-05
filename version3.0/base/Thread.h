#pragma once

#include <functional>
#include <memory>
#include <pthread.h>
#include <string>
#include <sys/syscall.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "CurrentThread.h"
#include "noncopyable.h"

using namespace std;

class Thread : noncopyable {
public:
	typedef function<void()> ThreadFunc;
	explicit Thread(const ThreadFunc& func, const string& name = string());
	~Thread();
	void start();//启动线程
	int join(); // return pthread_join() 阻塞等待子线程退出
	bool started() const { return started_; }//线程是否已经启动了
	pid_t getTid() const { return tid_; }//返回线程的真实tid
	const string& getName() const { return name_; }//返回线程的名字

private:
	static void* startThread(void* thread);//线程的入口函数,在这个函数中调用runInThread,然后runInthread又调用了回调函数ThreadFunc
	void runInThread();

private:
	bool       started_;  //线程是否已经启动了
	pthread_t  threadId_;//线程在进程中的pid，不同进程中线程的pid可以相同
	pid_t      tid_;      //线程的真实tid
	ThreadFunc func_;     //线程要回调的函数
	string     name_;     //线程的名字
};
