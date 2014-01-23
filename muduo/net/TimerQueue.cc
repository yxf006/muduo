#define __STDC_LIMIT_MACROS
#include <muduo/net/TimerQueue.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Timer.h>
#include <muduo/net/TimerId.h>

#include <boost/bind.hpp>

#include <sys/timerfd.h>

namespace muduo
{
namespace net
{
namespace detail
{

int createTimerfd()
{
  int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);// 调用timerfd_create创建定时器

  if (timerfd < 0)
  {
    LOG_SYSFATAL << "Failed in timerfd_create";
  }
  return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when) // 类型转换函数
{
  int64_t microseconds = when.microSecondsSinceEpoch()
                         - Timestamp::now().microSecondsSinceEpoch(); // 时间差
  if (microseconds < 100) // 超时时刻最小100ms
  {
    microseconds = 100;
  }
  struct timespec ts;
  ts.tv_sec = static_cast<time_t>(
      microseconds / Timestamp::kMicroSecondsPerSecond);
  ts.tv_nsec = static_cast<long>(
      (microseconds % Timestamp::kMicroSecondsPerSecond) * 1000);
  return ts;
}
// 调用read清除数据 否则会一直触发 LT 
void readTimerfd(int timerfd, Timestamp now)
{
  uint64_t howmany;
  ssize_t n = ::read(timerfd, &howmany, sizeof howmany); 
  LOG_TRACE << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
  if (n != sizeof howmany)
  {
    LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8";
  }
}
// 重设定时器fd
void resetTimerfd(int timerfd, Timestamp expiration)
{
  // wake up loop by timerfd_settime()
  struct itimerspec newValue;
  struct itimerspec oldValue;
  bzero(&newValue, sizeof newValue);
  bzero(&oldValue, sizeof oldValue);
  newValue.it_value = howMuchTimeFromNow(expiration); // 时间转换函数 计算到时的时间差
 
  // 调用timerfd_settime来重设时间
  int ret = ::timerfd_settime(timerfd, 0, &newValue, &oldValue);
  if (ret)
  {
    LOG_SYSERR << "timerfd_settime()";
  }
}}}}

using namespace muduo;
using namespace muduo::net;
using namespace muduo::net::detail;

TimerQueue::TimerQueue(EventLoop* loop)
  : loop_(loop),
    timerfd_(createTimerfd()),
    timerfdChannel_(loop, timerfd_),
    timers_(),
    callingExpiredTimers_(false)
{
  timerfdChannel_.setReadCallback(
      boost::bind(&TimerQueue::handleRead, this)); // 关注通道中的可读事件
  // we are always reading the timerfd, we disarm it with timerfd_settime.
  timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue()
{
  ::close(timerfd_);
  // do not remove channel, since we're in EventLoop::dtor();
  for (TimerList::iterator it = timers_.begin();
      it != timers_.end(); ++it)
  {
    delete it->second; // 删除定时器 等使用了智能指针就不再用手动删除了
  }
}

// 增加一个定时器 线程安全的异步调用 其他线程可以调用
TimerId TimerQueue::addTimer(const TimerCallback& cb, // 回调函数
                             Timestamp when,             // 超时事件
                             double interval)            // 间隔
{
  Timer* timer = new Timer(cb, when, interval);       // 1.创建一个定时器
  loop_->runInLoop(
      boost::bind(&TimerQueue::addTimerInLoop, this, timer)); // 2.将任务交给loop_对应的IO线程来处理 异步线程
  return TimerId(timer, timer->sequence()); // 定时器地址和序号
}

void TimerQueue::cancel(TimerId timerId)
{
  loop_->runInLoop(
      boost::bind(&TimerQueue::cancelInLoop, this, timerId)); // 线程安全
}

void TimerQueue::addTimerInLoop(Timer* timer)
{
  loop_->assertInLoopThread(); // 断言

  // 插入一个定时器 有可能会使得最早到期的定时器发生改变 它可能更早
  bool earliestChanged = insert(timer);

  if (earliestChanged) 
  {

    resetTimerfd(timerfd_, timer->expiration()); // 如果最早的到时时间改变重置定时器的超时时刻
  }
}

void TimerQueue::cancelInLoop(TimerId timerId)
{
  loop_->assertInLoopThread();

  assert(timers_.size() == activeTimers_.size());

  ActiveTimer timer(timerId.timer_, timerId.sequence_); // 活跃的定时器

  ActiveTimerSet::iterator it = activeTimers_.find(timer); // 从定时器set中找到
  if (it != activeTimers_.end()) // 找到要取消的定时器
  {
    size_t n = timers_.erase(Entry(it->first->expiration(), it->first)); // 从定时器列表中删除
    assert(n == 1); (void)n;

    delete it->first; //  删除这个定时器
    activeTimers_.erase(it);
  }
  else if (callingExpiredTimers_) // 不在列表中 已经到期了 并且正在调用回调函数的定时器
  {
    cancelingTimers_.insert(timer); // 插入到要cancel的定时器列表汇总
  }
  // 
  assert(timers_.size() == activeTimers_.size());
}

// 关注通道的可读事件 即某一个时刻定时器发生了超时
void TimerQueue::handleRead()
{
  loop_->assertInLoopThread(); // 断言I/O线程中

  Timestamp now(Timestamp::now());
  readTimerfd(timerfd_, now);      // 该定时器fd上有数据了 使用了LT 所以需要读走 避免一直触发

// 获取某一个时刻的超时列表 有可能某一时刻多个定时器超时
  std::vector<Entry> expired = getExpired(now);

  callingExpiredTimers_ = true;
  cancelingTimers_.clear();

  // safe to callback outside critical section
  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    it->second->run();         // 这里调用定时器到时 回调函数处理
  }
  callingExpiredTimers_ = false;

  reset(expired, now);         // 不是一次性定时器 需要重设
}

// rvo realease版本优化 所以不需要返回引用或指针
// RVO:即release版本中会对代码优化 使得返回一个对象不许调用拷贝构造函数 所以可以返回一个对象 不用指针或者引用
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
  assert(timers_.size() == activeTimers_.size());
  
  std::vector<Entry> expired;
  Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));

  // 返回第一个未到期的Timer的迭代器
  // 即*end >= sentry 从而 end->firt > now
  // 由于Entry是pair类型 所以还有比较后面的值 而sentry是最大值UINTPTR_MAX哨兵值
  // 保证了得到的比当前的大
  // lower_bound 是大于等于 upper_bound是大于
  TimerList::iterator end = timers_.lower_bound(sentry);
  assert(end == timers_.end() || now < end->first);

  // 将到期的定时器插入到expired中 STL中的区间都是闭开区间
  // 从timers_中移除到期的定时器
  // copy函数调用来将到时的定时器保存到expired vector中
  // 这里直接调用 不是显式的一个个插入
  std::copy(timers_.begin(), end, back_inserter(expired));
  timers_.erase(timers_.begin(), end);

  // 从activeTimers_中移除到期的定时器
  for (std::vector<Entry>::iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());
    size_t n = activeTimers_.erase(timer);
    assert(n == 1); (void)n;
  }

  assert(timers_.size() == activeTimers_.size());

  return expired;
}

// 重设timefd对应的时间timer
void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now)
{
  Timestamp nextExpire;

  for (std::vector<Entry>::const_iterator it = expired.begin();
      it != expired.end(); ++it)
  {
    ActiveTimer timer(it->second, it->second->sequence());

	// 如果是重复的定时器并且是未取消的定时器(能在活动的队列中找到) 则重启定时器
    if (it->second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end())
    {
      it->second->restart(now); // 重新计算下一个超时时刻
      insert(it->second);       // 将它插入到定时器队列中
    }
    else
    {
      // FIXME move to a free list 一次性定时器不能重置 删除该定时器
      delete it->second;       // FIXME: no delete please
    }
  }
  
  if (!timers_.empty())    // 得到下一次定时器的时间
  {
    nextExpire = timers_.begin()->second->expiration();
  }

  if (nextExpire.valid())  // 如果下次定时器的时间是合法的 重设timefd对应的触发时间
  {
    resetTimerfd(timerfd_, nextExpire);
  }
}

bool TimerQueue::insert(Timer* timer)
{
  loop_->assertInLoopThread(); 		    // 只能在IO线程中使用
  assert(timers_.size() == activeTimers_.size());
  
  bool earliestChanged = false;		    // 最早到期时间是否改变
  Timestamp when = timer->expiration(); // when保存在timer的expiration中
  TimerList::iterator it = timers_.begin(); 
  // 使用set来实现 默认从小到大排列时间戳 map不能用但是可以用multimap

  // 空队列或者更早的到期时间 最早到期时间发生改变
  if (it == timers_.end() || when < it->first) 
  {
    earliestChanged = true;
  }
  {// 插入到timer_中
    std::pair<TimerList::iterator, bool> result
      = timers_.insert(Entry(when, timer));
    assert(result.second); (void)result;
  }
  {// 插入到activeTimer_中
    std::pair<ActiveTimerSet::iterator, bool> result
      = activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    assert(result.second); (void)result;
  }

  assert(timers_.size() == activeTimers_.size());
  return earliestChanged;
}
