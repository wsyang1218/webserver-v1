#include "Thread.h"

Thread::Thread(const ThreadFunc& func, const std::string& n) : 
    started_(false), threadId_(0), tid_(0), func_(func), name_(n) {}

Thread::~Thread() {}

void Thread::start() {
	assert(!started_);
	started_ = true;
	int err = pthread_create(&threadId_, NULL, startThread, this);//不能直接传入runInThread，因为runInThread带有一个隐含的this指针
	if (err != 0)
	{
		fprintf(stderr, "pthread_create error:%s\n", strerror(err));
		exit(-1);
	}
}

int Thread::join() {
	assert(started_);
	return pthread_join(threadId_, NULL);
	return 0;
}

void* Thread::startThread(void* obj) {
	//startThread是静态函数，不能直接调用runInThread
	Thread* thread = static_cast<Thread*>(obj);
	thread->runInThread();
	//thread->func_();//直接这么写也可以
	return NULL;
}

void Thread::runInThread() {
	tid_ = CurrentThread::tid();
	if (name_.empty())
		CurrentThread::t_threadName = "empty";
	else
		CurrentThread::t_threadName = name_.data();
	func_();//调用回调函数
	CurrentThread::t_threadName = "finished";
}
