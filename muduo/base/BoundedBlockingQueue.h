#ifndef MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
#define MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/circular_buffer.hpp>
#include <boost/noncopyable.hpp>
#include <assert.h>

namespace muduo
{

template<typename T>
class BoundedBlockingQueue : boost::noncopyable
{
 public:
  explicit BoundedBlockingQueue(int maxSize)
    : mutex_(),
      notEmpty_(mutex_),
      notFull_(mutex_),
      queue_(maxSize)
  {
  }

  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    while (queue_.full())
    {
      notFull_.wait(); // 队列满了不能写入 阻塞等待
    }
    assert(!queue_.full());
    queue_.push_back(x);
    notEmpty_.notify(); //  写入数据不为空了
  }

  T take()
  {
    MutexLockGuard lock(mutex_);
    while (queue_.empty()) // 队列为空不能取出 阻塞等待
    {
      notEmpty_.wait();
    }
    assert(!queue_.empty());
    T front(queue_.front());
    queue_.pop_front();
    notFull_.notify(); // 取出数据 通知队列不满可以写入了
    return front;
  }

  bool empty() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.empty();
  }

  bool full() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.full();
  }

  size_t size() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

  size_t capacity() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.capacity();
  }
// 本质就是生产者-消费者模型
 private:
  mutable MutexLock          mutex_;
  Condition                  notEmpty_; // 判断是否非空 非空可读
  Condition                  notFull_;  // 判断是否不满 不满可写
  boost::circular_buffer<T>  queue_;    // 使用了boost库的环形缓冲区
};

}

#endif  // MUDUO_BASE_BOUNDEDBLOCKINGQUEUE_H
