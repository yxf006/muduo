// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/CountDownLatch.h>

using namespace muduo;

CountDownLatch::CountDownLatch(int count)
  : mutex_(),
    condition_(mutex_),
    count_(count)
{
}

void CountDownLatch::wait() // 当count_>0表示还有线程没有启动完成wait函数阻塞 继续等待到全部启动完成了
{
  MutexLockGuard lock(mutex_);
  while (count_ > 0) {
    condition_.wait();
  }
}

void CountDownLatch::countDown() // 每有一个线程完成启动 调用一次 次数减一 知道所有的都启动了
{
  MutexLockGuard lock(mutex_);
  --count_;
  if (count_ == 0) {
    condition_.notifyAll(); // 线程全部启动了 通知wait函数不用等待了 不用再阻塞了
  }
}

int CountDownLatch::getCount() const // 返回等待的数量
{
  MutexLockGuard lock(mutex_); // 这里会调用unlock改变mutex_的状态 这个是对mutex_的RAII封装
  return count_;
}

