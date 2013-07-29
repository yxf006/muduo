// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_MUTEX_H
#define MUDUO_BASE_MUTEX_H

#include <muduo/base/CurrentThread.h>
#include <boost/noncopyable.hpp>
#include <assert.h>
#include <pthread.h>

namespace muduo
{

class MutexLock : boost::noncopyable          // 对Mutex的封装
{
 public:
  MutexLock()
    : holder_(0)
  {
    int ret = pthread_mutex_init(&mutex_, NULL); // init
    assert(ret == 0); (void) ret;
  }

  ~MutexLock()
  {
    assert(holder_ == 0);                     // 使用assert提高程序的健壮性
    int ret = pthread_mutex_destroy(&mutex_); // destory
    assert(ret == 0); (void) ret;
  }

  bool isLockedByThisThread()          		  // 是否锁住的是当前对象
  {
    return holder_ == CurrentThread::tid();
  }

  void assertLocked()
  {
    assert(isLockedByThisThread());
  }

  // internal usage

  void lock()
  {
    pthread_mutex_lock(&mutex_);              // lock
    holder_ = CurrentThread::tid();           // id是当前线程的tid
  }

  void unlock()
  {
    holder_ = 0;
    pthread_mutex_unlock(&mutex_);            // unlock set holder_(0)
  }

  pthread_mutex_t* getPthreadMutex() /* non-const */
  {
    return &mutex_;  						  // 返回的是地址
  }

 private:

  pthread_mutex_t mutex_;
  pid_t holder_;
};

// 对MutexLock进行封装 利用类的析构函数可以防止我们解锁

class MutexLockGuard : boost::noncopyable 
{
 public:
  explicit MutexLockGuard(MutexLock& mutex)
    : mutex_(mutex)
  {
    mutex_.lock();
  }

  ~MutexLockGuard()
  {
    mutex_.unlock();
  }

 private:

  MutexLock& mutex_; // 类的关系的关联 不负责释放mutex_
};

}

// Prevent misuse like:
// MutexLockGuard(mutex_);
// A tempory object doesn't hold the lock for long! 一个临时对象不能hold锁很长时间
#define MutexLockGuard(x) error "Missing guard object name" // 避免构造一个匿名的对象

#endif  // MUDUO_BASE_MUTEX_H
