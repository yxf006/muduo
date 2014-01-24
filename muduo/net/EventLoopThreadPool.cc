#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;


EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop)
  : baseLoop_(baseLoop),
    started_(false),
    numThreads_(0),
    next_(0)
{
}

EventLoopThreadPool::~EventLoopThreadPool()
{
  // Don't delete loop, it's stack variable
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb)
{
  assert(!started_);
  baseLoop_->assertInLoopThread();

  started_ = true;

  for (int i = 0; i < numThreads_; ++i)
  {
    EventLoopThread* t = new EventLoopThread(cb);
    threads_.push_back(t); // 压入创建的IO线程
    loops_.push_back(t->startLoop()); // 启动EventLoopThread线程 在进入事件循环之前 会调用cb
  }
  if (numThreads_ == 0 && cb)
  {
	  //只有一个线程 EventLoop 在这个EventLoop进入事件循环之前 调用cb
    cb(baseLoop_);
  }
}

// 当新的连接到来时 使用一个EventLoop来处理
EventLoop* EventLoopThreadPool::getNextLoop()
{
  baseLoop_->assertInLoopThread();
  EventLoop* loop = baseLoop_; // 主线程

// baseLoop_就是mainReactor如果为空 直接返回它 否则做如下处理
  if (!loops_.empty())
  {
    // round-robin
    loop = loops_[next_]; // 如果不只一个线程 则取出一个事件循环EventLoop
    ++next_;
    if (implicit_cast<size_t>(next_) >= loops_.size())
    {
      next_ = 0;
    }
  }

  return loop; // 如果只有一个线程这里返回的就是主线程
}

