#ifndef MUDUO_NET_EVENTLOOPTHREADPOOL_H
#define MUDUO_NET_EVENTLOOPTHREADPOOL_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <vector>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace muduo
{

namespace net
{

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThreadPool(EventLoop* baseLoop);
  ~EventLoopThreadPool();
  void setThreadNum(int numThreads) { numThreads_ = numThreads; }
  void start(const ThreadInitCallback& cb = ThreadInitCallback());
  EventLoop* getNextLoop();

 private:

  EventLoop* baseLoop_; // 主线程
  bool started_;
  int numThreads_; // 线程数
  int next_;       // 当新连接到来 所选择的EventLoop对象下表
  boost::ptr_vector<EventLoopThread> threads_; // EventLoopThread IO线程列表  注意这里使用了boost的ptr_vector容器
  std::vector<EventLoop*> loops_;              // EventLoop列表 栈上对象 不需要手动销毁
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREADPOOL_H
