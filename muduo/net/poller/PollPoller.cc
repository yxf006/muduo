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

// 调用系统 ::poll
Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
 //  将返回的可读写事件保存在 pollfds_ 这个vector中
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
// 将可读写事件填充到fd对应的通道中
void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList* activeChannels) const
{
// 遍历通道中的 fd 填充到通道中
  for (PollFdList::const_iterator pfd = pollfds_.begin();
                                           pfd != pollfds_.end() && numEvents > 0; ++pfd)
  {
    if (pfd->revents > 0) // 产生了事件 返回事件 
    {
      --numEvents;        // 处理事件
      // 需要先调用 updateChannel 再找对应的 Channel
      ChannelMap::const_iterator ch = channels_.find(pfd->fd);// 通过 fd 查找通道

      assert(ch != channels_.end());

      Channel* channel = ch->second;      // 得到通道
      assert(channel->fd() == pfd->fd);

      channel->set_revents(pfd->revents);
      // pfd->revents = 0;

      activeChannels->push_back(channel); // 将通道压入到活动通道中 ChannelList通道列表在父类中定义
    }
  }
}

// 注册某个fd的可读可写事件即向通道中注册事件 或者 更新
void PollPoller::updateChannel(Channel* channel)
{
  Poller::assertInLoopThread(); // EventLoop对应thread
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
  
  if (channel->index() < 0) //  新的通道 数组中不存在
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

    channels_[pfd.fd] = channel; //  在map中插入 这个fd和对应的通道
  }
  else
  {
    // update existing one 更新通道关注的事件
    assert(channels_.find(channel->fd()) != channels_.end()); // 已经有的通道可以找到
    assert(channels_[channel->fd()] == channel);
	
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
	
    struct pollfd& pfd = pollfds_[idx];
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
	
    pfd.events = static_cast<short>(channel->events()); // 更新fd事件
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

// 移除map中 fd对应的IO通道
void PollPoller::removeChannel(Channel* channel)
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

  size_t n = channels_.erase(channel->fd()); // 用 key 来从 map 中移除
  assert(n == 1); (void)n;
  
  // 从fd 数组中移除fd
  if (implicit_cast<size_t>(idx) == pollfds_.size()-1) //
  {
    pollfds_.pop_back(); // 是最后一个 从fd数组中pop移除该fd
  }
  else // 不是最后一个
  {
    // 这里移除的算法复杂度为O(1) 由于没有顺序关系所以
	// 将带移除的元素与最后一个元素交换 再pop_back就好
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

