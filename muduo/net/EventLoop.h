// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOP_H
#define MUDUO_NET_EVENTLOOP_H

#include <vector>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/TimerId.h>

namespace muduo
{
namespace net
{

class Channel;
class Poller;
class TimerQueue;

///
/// Reactor, at most one per thread.
///
/// This is an interface class, so don't expose too much details.
class EventLoop : boost::noncopyable
{
 public:
  typedef boost::function<void()> Functor;

  EventLoop();
  ~EventLoop();  // force out-line dtor, for scoped_ptr members.

  ///
  /// Loops forever. 不能跨线程调用 只能在创建该对象的线程中调用
  ///
  /// Must be called in the same thread as creation of the object.
  ///
  void loop();

  void quit();

  ///
  /// Time when poll returns, usually means data arrivial.
  ///
  Timestamp pollReturnTime() const { return pollReturnTime_; }

  int64_t iteration() const { return iteration_; }

  /// Runs callback immediately in the loop thread.
  /// It wakes up the loop, and run the cb.
  /// If in the same loop thread, cb is run within the function.
  /// Safe to call from other threads.
  void runInLoop(const Functor& cb); // 线程的异步调用
  
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
  void queueInLoop(const Functor& cb);

  // timers 定时器

  ///
  /// Runs callback at 'time'.
  /// Safe to call from other threads.
  ///
  TimerId runAt(const Timestamp& time, const TimerCallback& cb);
  ///
  /// Runs callback after @c delay seconds.
  /// Safe to call from other threads.
  ///
  TimerId runAfter(double delay, const TimerCallback& cb);
  ///
  /// Runs callback every @c interval seconds.
  /// Safe to call from other threads.
  ///
  TimerId runEvery(double interval, const TimerCallback& cb);
  ///
  /// Cancels the timer.
  /// Safe to call from other threads.
  ///
  void cancel(TimerId timerId);

  // internal usage
  void wakeup();
  void updateChannel(Channel* channel); // 在POLLER中注册或者更新通道
  void removeChannel(Channel* channel); // 移除

  // pid_t threadId() const { return threadId_; }
  void assertInLoopThread()
  {
    if (!isInLoopThread())
    {
      abortNotInLoopThread();
    }
  }

  // 对比tid是否相等
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  // bool callingPendingFunctors() const { return callingPendingFunctors_; }
  bool eventHandling() const { return eventHandling_; }

  static EventLoop* getEventLoopOfCurrentThread();

 private:
  void abortNotInLoopThread();
  void handleRead();          // waked up
  void doPendingFunctors();

  void printActiveChannels() const; // DEBUG

  typedef std::vector<Channel*> ChannelList; // eventLoop与channel的关系是一对多 聚合关系

  bool looping_;  // 是否处于事件循�分�atomic
  bool quit_;     // 是否离开事件循环 atomic
  bool eventHandling_;  /* atomic */
  bool callingPendingFunctors_; /* atomic */
  
  int64_t iteration_;
  const pid_t threadId_;      // 每一个EventLoop对应一个线程 这个记录对应的线程ID
  Timestamp pollReturnTime_;  // 时间戳
  
  boost::scoped_ptr<Poller> poller_;         // Poller
  boost::scoped_ptr<TimerQueue> timerQueue_; // 定时器队列
  
  int wakeupFd_; // 用于eventfd 实现线程间通信
  // unlike in TimerQueue, which is an internal class,
  // we don't expose Channel to client.
  // 该通道将会纳入poller_来管理 只负责这个channel的生存期
  boost::scoped_ptr<Channel> wakeupChannel_; 
  ChannelList activeChannels_;               // 事件通道
  Channel* currentActiveChannel_;            // 正在处理的活动通道
  MutexLock mutex_;
  std::vector<Functor> pendingFunctors_; // @BuardedBy mutex_ 执行一些计算任务
};

}
}
#endif  // MUDUO_NET_EVENTLOOP_H
