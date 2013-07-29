// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMERQUEUE_H
#define MUDUO_NET_TIMERQUEUE_H

#include <set>
#include <vector>

#include <boost/noncopyable.hpp>

#include <muduo/base/Mutex.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Channel.h>

namespace muduo
{
namespace net
{

class EventLoop;
class Timer;
class TimerId;

///
/// A best efforts timer queue.
/// No guarantee that the callback will be on time.
///
class TimerQueue : boost::noncopyable
{
 public:
  TimerQueue(EventLoop* loop);
  ~TimerQueue();

  ///
  /// Schedules the callback to be run at given time,
  /// repeats if @c interval > 0.0.
  ///
  /// Must be thread safe. Usually be called from other threads.
  // 一定是线程安全的 可以跨线程调用 通常情况下被其他线程调用
  TimerId addTimer(const TimerCallback& cb,
                   Timestamp when,
                   double interval);

  void cancel(TimerId timerId);

 private:

  // FIXME: use unique_ptr<Timer> instead of raw pointers.
  // unique_ptr是C++ 11标准的一个独享所有权的智能指针 这里用裸指针
  // 无法得到指向同一对象的两个unique_ptr指针
  // 但可以进行移动构造与移动赋值操作 即所有权可以移动到另一个对象(而非拷贝构造)
  typedef std::pair<Timestamp, Timer*> Entry;     // 保存的东西相同 按时间排序
  typedef std::set<Entry> TimerList;
  
  typedef std::pair<Timer*, int64_t> ActiveTimer; // 按地址排序
  typedef std::set<ActiveTimer> ActiveTimerSet;

// 一下成员函数只可能在其所属的IO线程调用 所以不需要加锁
// 服务器性能杀手之一是所竞争 所以尽可能较少所竞争
  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId);
  // called when timerfd alarms
  void handleRead();
  // move out all expired timers

  // 返回超时的定时器列表
  std::vector<Entry> getExpired(Timestamp now);
  void reset(const std::vector<Entry>& expired, Timestamp now);

  bool insert(Timer* timer);

  EventLoop* loop_;                // 所属EventLooop
  const int timerfd_;
  Channel timerfdChannel_;
  // Timer list sorted by expiration 按时间顺序
  TimerList timers_;

  // for cancel()
  ActiveTimerSet activeTimers_; // timer_和activeTimers_保存的是相同的数据 一个按照时间排序 一个按照对象地址排序
  bool callingExpiredTimers_; /* atomic */
  ActiveTimerSet cancelingTimers_; // 保存的是被取消的定时器
};

}
}
#endif  // MUDUO_NET_TIMERQUEUE_H
