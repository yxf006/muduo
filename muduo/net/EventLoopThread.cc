// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThread.h>

#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;


EventLoopThread::EventLoopThread(const ThreadInitCallback& cb)
  : loop_(NULL),
    exiting_(false),
    thread_(boost::bind(&EventLoopThread::threadFunc, this)),
    mutex_(),
    cond_(mutex_),
    callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  loop_->quit(); // 退出 IO 线程 让IO线程的loop循环退出 从而退出了IO线程
  thread_.join();// 等待线程退出
}

EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  thread_.start();

  {
    MutexLockGuard lock(mutex_);
    while (loop_ == NULL) // loop为空一直等待 有IO事件发生
    {
      cond_.wait(); // 等待notify
    }
  }

  return loop_;
}

void EventLoopThread::threadFunc()
{
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);
  }

  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop; 
	// 这里loop不为空了 指向一个栈上的对象 这个函数结束时 这个指针就无效了
    // threadFunc函数退出 就意味着线程退出了 EventLoopThread对象也就没有存在的价值
    // 整个程序都结束了 所以销毁不销毁都没什么关系
    cond_.notify(); // 这里notify
  }

  loop.loop();
  //assert(exiting_);
}

