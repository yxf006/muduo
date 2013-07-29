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
  /// Loops forever. ²»ÄÜ¿çÏß³Ìµ÷ÓÃ Ö»ÄÜÔÚ´´½¨¸Ã¶ÔÏóµÄÏß³ÌÖĞµ÷ÓÃ
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
  void runInLoop(const Functor& cb); // Ïß³ÌµÄÒì²½µ÷ÓÃ
  
  /// Queues callback in the loop thread.
  /// Runs after finish pooling.
  /// Safe to call from other threads.
  void queueInLoop(const Functor& cb);

  // timers ¶¨Ê±Æ÷

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
  void updateChannel(Channel* channel); // ÔÚPOLLERÖĞ×¢²á»òÕß¸üĞÂÍ¨µÀ
  void removeChannel(Channel* channel); // ÒÆ³ı

  // pid_t threadId() const { return threadId_; }
  void assertInLoopThread()
  {
    if (!isInLoopThread())
    {
      abortNotInLoopThread();
    }
  }

  // ¶Ô±ÈtidÊÇ·ñÏàµÈ
  bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
  // bool callingPendingFunctors() const { return callingPendingFunctors_; }
  bool eventHandling() const { return eventHandling_; }

  static EventLoop* getEventLoopOfCurrentThread();

 private:
  void abortNotInLoopThread();
  void handleRead();          // waked up
  void doPendingFunctors();

  void printActiveChannels() const; // DEBUG

  typedef std::vector<Channel*> ChannelList; // eventLoopÓëchannelµÄ¹ØÏµÊÇÒ»¶Ô¶à ¾ÛºÏ¹ØÏµ

  bool looping_; /* atomic */ // ÊÇ·ñ´¦ÓÚÊÂ¼şÑ­» ·ÖĞ
  bool quit_; /* atomic */    // ÊÇ·ñÀë¿ªÊÂ¼şÑ­»·
  bool eventHandling_;  /* atomic */
  bool callingPendingFunctors_; /* atomic */
  
  int64_t iteration_;
  const pid_t threadId_;      // Ã¿Ò»¸öEventLoop¶ÔÓ¦Ò»¸öÏß³Ì Õâ¸ö¼ÇÂ¼¶ÔÓ¦µÄÏß³ÌID
  Timestamp pollReturnTime_;  // Ê±¼ä´Á
  
  boost::scoped_ptr<Poller> poller_;         // Poller
  boost::scoped_ptr<TimerQueue> timerQueue_; // ¶¨Ê±Æ÷¶ÓÁĞ
  
  int wakeupFd_; // ÓÃÓÚeventfd ÊµÏÖÏß³Ì¼äÍ¨ĞÅ
  // unlike in TimerQueue, which is an internal class,
  // we don't expose Channel to client.
  // ¸ÃÍ¨µÀ½«»áÄÉÈëpoller_À´¹ÜÀí Ö»¸ºÔğÕâ¸öchannelµÄÉú´æÆÚ
  boost::scoped_ptr<Channel> wakeupChannel_; 
  ChannelList activeChannels_;               // ÊÂ¼şÍ¨µÀ
  Channel* currentActiveChannel_;            // ÕıÔÚ´¦ÀíµÄ»î¶¯Í¨µÀ
  MutexLock mutex_;
  std::vector<Functor> pendingFunctors_; // @BuardedBy mutex_ Ö´ĞĞÒ»Ğ©¼ÆËãÈÎÎñ
};

}
}
#endif  // MUDUO_NET_EVENTLOOP_H
