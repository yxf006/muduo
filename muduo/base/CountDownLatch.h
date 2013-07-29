// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_COUNTDOWNLATCH_H
#define MUDUO_BASE_COUNTDOWNLATCH_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>

namespace muduo
{
// 可以用于所有子线程等待主线程发起起跑 
// 也可以用于主线程等待所有子线程初始化完毕才看是工作
class CountDownLatch : boost::noncopyable // 对MutexLock Condition的封装 他们需要配合使用
{
 public:

  explicit CountDownLatch(int count);

  void wait();

  void countDown();

  int getCount() const;

 private:
  mutable MutexLock mutex_;
  // 这里加mutable的原因是   int getCount() const; 
  // 本身不能改变变量需要加mutable才能改变mutex_来改变
  // 在C++中，mutable也是为了突破const的限制而设置的。
  // 被mutable修饰的变量，将永远处于可变的状态，即使在一个const函数中。
  Condition condition_;
  int count_;
};

}
#endif  // MUDUO_BASE_COUNTDOWNLATCH_H
