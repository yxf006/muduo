// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/ThreadPool.h>

#include <muduo/base/Exception.h>

#include <boost/bind.hpp>
#include <assert.h>
#include <stdio.h>

using namespace muduo;

ThreadPool::ThreadPool(const string& name)
  : mutex_(),
    cond_(mutex_),
    name_(name),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop();
  }
}

void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;
  threads_.reserve(numThreads); // must
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i);
    threads_.push_back(new muduo::Thread( // 创建线程对象且加入threads_(ptr_vector保存了指针的vector)
          boost::bind(&ThreadPool::runInThread, this), name_+id));// 同时保存了线程运行函数
    threads_[i].start(); // start runInThread函数启动
  }
}

void ThreadPool::stop()
{
  {
  MutexLockGuard lock(mutex_);
  running_ = false;
  cond_.notifyAll();
  }
  for_each(threads_.begin(),
           threads_.end(),
           boost::bind(&muduo::Thread::join, _1)); // wait for every pthread stop
}

void ThreadPool::run(const Task& task) // 运行任务 Task(function)
{
  if (threads_.empty())     // 线程队列为空 没有线程运行 则直接用主线程(进程)运行
  {
    task();
  }
  else                      // 否则将任务添加到任务队列通知所有的等待线程
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(task); 
    cond_.notify(); // 通知有task了 可以取出执行 实现线程间的同步
  }
}

ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  while (queue_.empty() && running_) // 任务到来 或者线程池结束 不再等待跳出循环
  {
    cond_.wait(); // 有task了 不再阻塞
  }
  Task task;
  if(!queue_.empty())
  {
    task = queue_.front(); 	   // 取出任务
    queue_.pop_front();
  }
  return task;
}

void ThreadPool::runInThread() // 线程运行函数
{
  try
  {
    while (running_) 		// 线程池在运行
    {
      Task task(take());    // 从任务队列取出任务 没有任务会阻塞在take函数
      if (task)             // 任务非空 执行任务
      {
        task();
      }
    }
  }
  catch (const Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}

