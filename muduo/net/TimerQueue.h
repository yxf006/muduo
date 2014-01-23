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

class TimerQueue : boost::noncopyable
{
 public:
  TimerQueue(EventLoop* loop);
  ~TimerQueue();

  // 一定是线程安全的 可以跨线程调用 通常情况下被其他线程调用 简单的2个接口
  TimerId addTimer(const TimerCallback& cb,
                   Timestamp when,
                   double interval);

  void cancel(TimerId timerId);

 private:

  // unique_ptr是C++ 11标准的一个独享所有权的智能指针 这里用裸指针无法得到指向同一对象的两个unique_ptr指针
   // 但可以进行移动构造与移动赋值操作 即所有权可以移动到另一个对象(而非拷贝构造) 保存的东西相同 按时间排序

  typedef std::pair<Timestamp, Timer*> Entry;
  typedef std::set<Entry> TimerList;              // 按超时时间排序的
  
  typedef std::pair<Timer*, int64_t> ActiveTimer; // 按地址排序 地址和序号
  typedef std::set<ActiveTimer> ActiveTimerSet;   // 保存相同的东西 按地址安排

// 这里的成员函数只可能在其所属的IO线程调用 所以不需要加锁 服务器性能杀手之一是所竞争 所以尽可能较少所竞争
  void addTimerInLoop(Timer* timer);
  void cancelInLoop(TimerId timerId); // called when timerfd alarms
  void handleRead();// move out all expired timers

  std::vector<Entry> getExpired(Timestamp now); // 返回超时的定时器列表
  void reset(const std::vector<Entry>& expired, Timestamp now); // 重设定时器

  bool insert(Timer* timer);

  EventLoop* loop_;         // 所属EventLooop
  const int timerfd_;      // 这里的定时器是按照fd来处理的
  Channel timerfdChannel_;  // 定时器读写事件到来的处理通道
  TimerList timers_;  // Timer list sorted by expiration 按超时的时间顺序

  // for cancel()
  ActiveTimerSet activeTimers_;    // timer_和activeTimers_保存的是相同的数据 一个按照时间排序 一个按照对象地址排序
  bool callingExpiredTimers_;
  ActiveTimerSet cancelingTimers_; // 保存的是被取消的定时器
};

}
}
#endif  // MUDUO_NET_TIMERQUEUE_H
