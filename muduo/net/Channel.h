// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <muduo/base/Timestamp.h>

namespace muduo
{
namespace net
{

class EventLoop;

///
/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor. 不拥有文件描述符fd
/// The file descriptor could be a socket,
/// an eventfd, a timerfd, or a signalfd
class Channel : boost::noncopyable // 用于注册或者回调IO事件
{
 public:
  typedef boost::function<void()> EventCallback;
  typedef boost::function<void(Timestamp)> ReadEventCallback;

  Channel(EventLoop* loop, int fd); // 一个channel只属于一个EventLoop
  ~Channel();

  void handleEvent(Timestamp receiveTime);
  
  void setReadCallback(const ReadEventCallback& cb) // 几个读写回调函数
  { readCallback_ = cb; }
  
  void setWriteCallback(const EventCallback& cb)
  { writeCallback_ = cb; }
  
  void setCloseCallback(const EventCallback& cb)
  { closeCallback_ = cb; }
  
  void setErrorCallback(const EventCallback& cb)
  { errorCallback_ = cb; }

  /// Tie this channel to the owner object managed by shared_ptr,
  /// prevent the owner object being destroyed in handleEvent.
  void tie(const boost::shared_ptr<void>&);

  int fd() const { return fd_; }
  
  int events() const { return events_; }                    // 注册的事件
  
  void set_revents(int revt) { revents_ = revt; }           // used by pollers
  // int revents() const { return revents_; }
  bool isNoneEvent() const { return events_ == kNoneEvent; }

  void enableReading() { events_ |= kReadEvent; update(); } // 位操作来设置一些状态
  
  // void disableReading() { events_ &= ~kReadEvent; update(); }
  void enableWriting() { events_ |= kWriteEvent; update(); }
  
  void disableWriting() { events_ &= ~kWriteEvent; update(); }
  
  void disableAll() { events_ = kNoneEvent; update(); }     // 在remove前要先调用这个
  
  bool isWriting() const { return events_ & kWriteEvent; } // kWriteEvent(POLLOUT)

  // for Poller
  int index() { return index_; }
  void set_index(int idx) { index_ = idx; }

  // for debug
  string reventsToString() const;

  void doNotLogHup() { logHup_ = false; }

  EventLoop* ownerLoop() { return loop_; }
  void remove();

 private:
  void update();
  void handleEventWithGuard(Timestamp receiveTime);

  static const int kNoneEvent;  // Poller的可读写事件
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_;   		// 所属的EventLoop
  const int  fd_;     		// 所关注的fd 不负责关闭fd
  int        events_; 		// 关注的事件
  int        revents_;		// poll/epoll 返回的事件
  int        index_;  		// used by Poller. 表示在poll的事件数组中的序号 epoll中的状态
  bool       logHup_; 		// for POLLHUP

  boost::weak_ptr<void> tie_;      // 这是一个弱引用 用于对象生命期的控制 TcpConnection
  bool tied_;         			   // 生存期的控制
  bool eventHandling_;			   // 是否处于事件处理中  
  ReadEventCallback readCallback_; // 读回调函数
  EventCallback writeCallback_;    // 写回调函数
  EventCallback closeCallback_;    // 关闭回调函数
  EventCallback errorCallback_;    // 错误回调函数
};

}
}
#endif  // MUDUO_NET_CHANNEL_H
