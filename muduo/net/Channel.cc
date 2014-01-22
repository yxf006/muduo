// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

// 初始化
const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI; // IN or POLLPRI 带外数据 紧急数据
const int Channel::kWriteEvent = POLLOUT;

Channel::Channel(EventLoop* loop, int fd__)
  : loop_(loop),
    fd_(fd__),
    events_(0),
    revents_(0),
    index_(-1),
    logHup_(true),
    tied_(false),
    eventHandling_(false)
{
}

Channel::~Channel()
{
  assert(!eventHandling_);
}

void Channel::tie(const boost::shared_ptr<void>& obj) 
{
  tie_ = obj;    // 这里是弱引用 不会将shared_ptr的引用计数加1
  tied_ = true;
}

void Channel::update()
{
  loop_->updateChannel(this); // 调用 Poller 的 updateChannel
}

void Channel::remove()
{
  assert(isNoneEvent());
  loop_->removeChannel(this);// 调用 Poller 的 updateChannel 在PollPoller类中实现
}

void Channel::handleEvent(Timestamp receiveTime)   // 可读写事件处理函数
{
  boost::shared_ptr<void> guard;   
  if (tied_)
  {
    guard = tie_.lock(); // 这里是对弱指针的一个提升
    if (guard)
    {
      handleEventWithGuard(receiveTime); // 调用提前注册的回调函数处理读写事件
    }
  }
  else
  {
    handleEventWithGuard(receiveTime);
  }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
  eventHandling_ = true;
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN)) // 判断返回的事件 为挂断 close
  {
    if (logHup_)
    {
      LOG_WARN << "Channel::handle_event() POLLHUP";
    }
    if (closeCallback_) closeCallback_();           // 调用对应的事件处理函数
  }

  if (revents_ & POLLNVAL) 						    // 文件描述符fd没打开或者非法
  {
    LOG_WARN << "Channel::handle_event() POLLNVAL";
  }

  if (revents_ & (POLLERR | POLLNVAL))             // 错误的
  {
    if (errorCallback_) errorCallback_();
  }
  // POLLRDHUP 关闭连接或者关闭半连接
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP))   // 对等放调用close关闭连接 会受到POLLRDHUP POLLPRI(带外数据)
  {
    if (readCallback_) readCallback_(receiveTime); // 产生可读事件 调用读函数
  }
  if (revents_ & POLLOUT)
  {
    if (writeCallback_) writeCallback_();  // 可写事件的产生 调用写的回调函数
  }
  eventHandling_ = false;
}

string Channel::reventsToString() const
{
  std::ostringstream oss;
  oss << fd_ << ": ";
  if (revents_ & POLLIN)
    oss << "IN ";
  if (revents_ & POLLPRI)
    oss << "PRI ";
  if (revents_ & POLLOUT)
    oss << "OUT ";
  if (revents_ & POLLHUP)
    oss << "HUP ";
  if (revents_ & POLLRDHUP)
    oss << "RDHUP ";
  if (revents_ & POLLERR)
    oss << "ERR ";
  if (revents_ & POLLNVAL)
    oss << "NVAL ";

  return oss.str().c_str();
}
