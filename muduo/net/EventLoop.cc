// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoop.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Singleton.h>
#include <muduo/net/Channel.h>
#include <muduo/net/Poller.h>
#include <muduo/net/SocketsOps.h>
#include <muduo/net/TimerQueue.h>

#include <boost/bind.hpp>

#include <signal.h>
#include <sys/eventfd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{
__thread EventLoop* t_loopInThisThread = 0; // 当前指针对象 加__thread表示线程存储

const int kPollTimeMs = 10000; // 10s

// eventfd用来线程间 通信 不用pipe socketpair
int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
 public:
  IgnoreSigPipe()
  {
    ::signal(SIGPIPE, SIG_IGN);
    LOG_TRACE << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj;
}

EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

EventLoop::EventLoop()
  : looping_(false),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    iteration_(0),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    timerQueue_(new TimerQueue(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)), // 这里创建了一个eventfd通道
    currentActiveChannel_(NULL)
{
  LOG_TRACE << "EventLoop created " << this << " in thread " << threadId_;
  if (t_loopInThisThread) // 如果当前线程已经创建了EventLoop对象 LOG_FATAL
  {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  }
  else
  {
    t_loopInThisThread = this;
  }
  wakeupChannel_->setReadCallback(
      boost::bind(&EventLoop::handleRead, this)); // 需要读走否则会一直触发
  // we are always reading the wakeupfd
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
  ::close(wakeupFd_); // 关闭wakeupFd
  t_loopInThisThread = NULL;
}

// 不能跨线程调用 只能在创建该对象的线程中调用
// 调用通道中注册的IO处理函数 处理超时事件
void EventLoop::loop()
{
  assert(!looping_);    // 断言是否处于事件循环中
  assertInLoopThread(); // 断言是否处于创建该对象的线程中
  looping_ = true;
  quit_ = false;
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_)
  {
    activeChannels_.clear();
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_); // 超时事件
    ++iteration_;
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels(); // 日志的处理
    }
    // TODO sort channel by priority 通过优先权排序通道
    eventHandling_ = true; // true
    for (ChannelList::iterator it = activeChannels_.begin(); // 遍历活动通道进行处理
        it != activeChannels_.end(); ++it)
    {
      currentActiveChannel_ = *it;
      currentActiveChannel_->handleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = NULL;
    eventHandling_ = false; // false
    doPendingFunctors();    // 为了让IO线程也能执行一些计算任务
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

void EventLoop::quit() // 可以跨线程调用
{
  quit_ = true;
  if (!isInLoopThread())
  {
    wakeup();         // 唤醒别的线程
  }
}

// 在IO线程中执行某个回调函数 执行某个函数 该函数可以跨线程调用 
void EventLoop::runInLoop(const Functor& cb) // 在不加锁的情况下 保证线程安全
{
  if (isInLoopThread()) // 同步IO
  {
    cb();               // 如果是当前线程 调用同步cb
  }
  else                  // 异步用来计算  
  {
    queueInLoop(cb);// 如果是其他线程调用 则异步地将cb添加到队列中
  }
}

// 将任务添加到异步任务队列中
void EventLoop::queueInLoop(const Functor& cb)
{
  {
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(cb);
  }

// 如果不是当前IO线程 则需要唤醒当前线程 或者当前线程正在调用pending functor 也需要唤醒
// 只有当前IO线程的事件回调中调用queueInLooop才不需要唤醒
  if (!isInLoopThread() || callingPendingFunctors_)
  {
    wakeup();
  }
}

// 定时器函数 一次性定时器
TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb) 
{
  return timerQueue_->addTimer(cb, time, 0.0);
}

// 定时器函数 
TimerId EventLoop::runAfter(double delay, const TimerCallback& cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, cb);
}

// 间隔性定时器
TimerId EventLoop::runEvery(double interval, const TimerCallback& cb) // 定时器函数
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(cb, time, interval);
}

// 定时器函数
void EventLoop::cancel(TimerId timerId)  
{
  return timerQueue_->cancel(timerId);
}

void EventLoop::updateChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  poller_->removeChannel(channel);
}

void EventLoop::abortNotInLoopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

// 唤醒别的线程函数
void EventLoop::wakeup()
{
  uint64_t one = 1; // 8个字节的缓冲区 写入 1 wakeup
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

// 这是需要读走的原因是因为LT 不读的话 会一直触发
void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

// 不是简单地在临界区一次调用Functor 而是把回调列表swap到functors中 这样一方面减少了临界区的长度
// 意味不会阻塞其他线程的queueLoop()
// 另一方面也避免了死锁 因为Functor可能再次调用queueInLooop()
// 由于doPendingFunctors()调用的Functor可能再次调用queueInLoop(cb) 这时 queueInLoop()就必须wakeup()
// 否则新增的cb可能就不能及时调用了
// muduo 没有反复执行doPendingFunctors()直到pendingFunctors为空 这是有意的
// 否则IO线程可能陷入死循环 无法处理IO事件
void EventLoop::doPendingFunctors()
{
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  {
  MutexLockGuard lock(mutex_);
  functors.swap(pendingFunctors_); // 将pendFunctors_都添加到functors中 让它来执行
  }

  for (size_t i = 0; i < functors.size(); ++i)
  {
    functors[i]();
  }
  callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
  for (ChannelList::const_iterator it = activeChannels_.begin();
      it != activeChannels_.end(); ++it)
  {
    const Channel* ch = *it;
    LOG_TRACE << "{" << ch->reventsToString() << "} ";
  }
}

