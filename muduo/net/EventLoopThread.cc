#include <muduo/net/EventLoopThread.h>
#include <muduo/net/EventLoop.h>
#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb)
  : loop_(NULL),
    exiting_(false),
    thread_(boost::bind(&EventLoopThread::threadFunc, this)),
    mutex_(),
    cond_(mutex_),
    callback_(cb)
{
}

EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  loop_->quit(); // 退出 IO 线程 让IO线程的loop循环退出 从而退出了IO线程
  thread_.join();// 等待线程退出
}

EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  thread_.start();

  {
    MutexLockGuard lock(mutex_);
    while (loop_ == NULL) // loop为空一直等待 直到有IO事件发生
    {
      cond_.wait(); // 等待notify
    }
  }
  return loop_;
}

void EventLoopThread::threadFunc()
{
  EventLoop loop;

  if (callback_)
  {
    callback_(&loop);
  }
  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop; 

    // 这里指针指向了一个栈上对象 没有销毁的原因是：线程执行的时候调用这个线程执行函数
    // 当线程执行结束的时候 线程对象销毁了 系统会自动释放资源

    cond_.notify(); // 这里notify 事件循环有函数了 loop_不为空了
  }

  loop.loop();
  //assert(exiting_);
}

