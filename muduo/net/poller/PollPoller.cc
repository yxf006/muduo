// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/poller/PollPoller.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <muduo/net/Channel.h>

#include <assert.h>
#include <poll.h>

using namespace muduo;
using namespace muduo::net;

PollPoller::PollPoller(EventLoop* loop)
  : Poller(loop)
{
}

PollPoller::~PollPoller()
{
}

Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
  // XXX pollfds_ shouldn't change
  // 调用系统的poll 传入数组的首地址 关注这些fd的可读可写事件
  // 这里传入的是地址 而不是间接地址指针
  int numEvents = ::poll(&*pollfds_.begin(), polclfds_.size(), timeoutMs); 
  Timestamp now(Timestamp::now());
  if (numEvents > 0) // 如果大于0将这些事件返回到IO通道中
  {
    LOG_TRACE << numEvents << " events happended";
    fillActiveChannels(numEvents, activeChannels); // 如果有事件返回则将事件放入到通道中
  }
  else if (numEvents == 0) // timeout
  {
    LOG_TRACE << " nothing happended";
  }
  else
  {
    LOG_SYSERR << "PollPoller::poll()";
  }
  return now;
}

void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList* activeChannels) const
{
// 遍历通道中的fds
  for (PollFdList::const_iterator pfd = pollfds_.begin();
      pfd != pollfds_.end() && numEvents > 0; ++pfd)
  {
    if (pfd->revents > 0) // 产生了事件 返回事件 
    {
      --numEvents;        // 处理事件
      ChannelMap::const_iterator ch = channels_.find(pfd->fd);// 通过fd查找通道
      assert(ch != channels_.end());
      Channel* channel = ch->second;      // 得到通道
      assert(channel->fd() == pfd->fd);
      channel->set_revents(pfd->revents);
      // pfd->revents = 0;
      activeChannels->push_back(channel); // 将通道压入到活动通道中
    }
  }
}

// 注册某个fd的可读可写事件或者更新
void PollPoller::updateChannel(Channel* channel)
{
  Poller::assertInLoopThread(); // EventLoop对应thread
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
  
  if (channel->index() < 0) // 在数组中的位置未知 新的通道
  {
    // a new one, add to pollfds_ 
    assert(channels_.find(channel->fd()) == channels_.end());
    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    pollfds_.push_back(pfd);
    int idx = static_cast<int>(pollfds_.size())-1; // vector容量
    channel->set_index(idx);     // 设置位置
    channels_[pfd.fd] = channel; // 设置fd对应的channel_
  }
  else
  {
    // update existing one 更新通道
    assert(channels_.find(channel->fd()) != channels_.end()); // 已经有的通道可以找到
    assert(channels_[channel->fd()] == channel);
	
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
	
    struct pollfd& pfd = pollfds_[idx];
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
	
    pfd.events = static_cast<short>(channel->events()); // 更新事件
    pfd.revents = 0;

	// 将一个通道暂时更改为不关注事件 但是不从Poller中移除该通道
    if (channel->isNoneEvent())
    {
      // ignore this pollfd
      // 暂时忽略该文件描述符的事件 
      // 这里pfd.fd 可以直接设置为-1 这样设置是为了removeChannel优化
      pfd.fd = -channel->fd()-1; // -1为了处理0 经过2次的这样处理可以得到原来的值
    }
  }
}

void PollPoller::removeChannel(Channel* channel) // 移除fd对应的IO通道
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd();
  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);
  assert(channel->isNoneEvent());  // 断言没有事件
  
  int idx = channel->index();
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
  const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
  
  assert(pfd.fd == -channel->fd()-1 && pfd.events == channel->events());
  size_t n = channels_.erase(channel->fd()); // 用key来同通道map中移除 
  assert(n == 1); (void)n;
  
  if (implicit_cast<size_t>(idx) == pollfds_.size()-1) // 最后一个
  {
    pollfds_.pop_back();
  }
  else // 不是最后一个
  {
  // 这里移除的算法复杂度为O(1) 由于没有顺序关系所以 将带移除的元素与最后一个元素交换再pop_back就好
    int channelAtEnd = pollfds_.back().fd;
    iter_swap(pollfds_.begin()+idx, pollfds_.end()-1); // swap: iter_swap
    if (channelAtEnd < 0)
    {
      channelAtEnd = -channelAtEnd-1;
    }
    channels_[channelAtEnd]->set_index(idx); // 更新最后一个元素的index
    pollfds_.pop_back();
  }
}

