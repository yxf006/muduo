// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BLOCKINGQUEUE_H
#define MUDUO_BASE_BLOCKINGQUEUE_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>
#include <deque>
#include <assert.h>

namespace muduo
{

template<typename T>
class BlockingQueue : boost::noncopyable
{
 public:
  BlockingQueue()
    : mutex_(),
      notEmpty_(mutex_),
      queue_()
  {
  }

  void put(const T& x)
  {
    MutexLockGuard lock(mutex_); // 对队列进行保护
    queue_.push_back(x);
    notEmpty_.notify(); // TODO: move outside of lock 通知等待的线程 实现线程同步
  }

  T take()
  {
    MutexLockGuard lock(mutex_);
    // always use a while-loop, due to spurious wakeup
    while (queue_.empty())
    {
      notEmpty_.wait(); // 为空阻塞
    }
    assert(!queue_.empty());
    T front(queue_.front());
    queue_.pop_front();
    return front;
  }

  size_t size() const /* 可能有多个线程访问所以需要保护 */
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

 private:
  mutable MutexLock mutex_;    // Mutex mutable 因为有些成员函数需要释放锁 改变它的姿态
  Condition         notEmpty_;  // 条件变量
  std::deque<T>     queue_;     // 使用了标准库的deque<T>双端队列
};

}

#endif  // MUDUO_BASE_BLOCKINGQUEUE_H
