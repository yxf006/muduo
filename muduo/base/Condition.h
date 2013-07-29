// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_CONDITION_H
#define MUDUO_BASE_CONDITION_H

#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo
{

class Condition : boost::noncopyable
{
 public:
  explicit Condition(MutexLock& mutex)
    : mutex_(mutex)
  {
    pthread_cond_init(&pcond_, NULL);   // init
  }

  ~Condition()
  {
    pthread_cond_destroy(&pcond_);      // destroy
  }

  void wait()
  {
    pthread_cond_wait(&pcond_, mutex_.getPthreadMutex()); 
	// wait 做的3件事: 解锁 等待条件变量 加锁 
  }

  // returns true if time out, false otherwise.
  bool waitForSeconds(int seconds);

  void notify()
  {
    pthread_cond_signal(&pcond_);  // notify
  }

  void notifyAll()
  {
    pthread_cond_broadcast(&pcond_); // notifyAll 
  }

 private:
  MutexLock& mutex_;     // 条件变量与Mutex配合使用
  pthread_cond_t pcond_;
};

}
#endif  // MUDUO_BASE_CONDITION_H
