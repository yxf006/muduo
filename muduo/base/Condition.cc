// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Condition.h>

#include <errno.h>

// returns true if time out, false otherwise. 等待时间到了返回true
bool muduo::Condition::waitForSeconds(int seconds)
{
  struct timespec abstime;
  clock_gettime(CLOCK_REALTIME, &abstime);
  abstime.tv_sec += seconds;
  return ETIMEDOUT == pthread_cond_timedwait(&pcond_, \
  	mutex_.getPthreadMutex(), &abstime);

  // 如果是ET触发 pthread_cond_timedwait 等待一段时间后时间到了返回ETIMEDOUT
}

