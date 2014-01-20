// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include <boost/noncopyable.hpp>
#include <pthread.h>
#include <stdlib.h> // atexit

namespace muduo
{

template<typename T>
class Singleton : boost::noncopyable
{
 public:
  static T& instance()
  {
    pthread_once(&ponce_, &Singleton::init); // 保证了Singleton::init()函数只会调用一次 保证了线程安全
    return *value_;
  }

 private:
  Singleton();
  ~Singleton();

  static void init()
  {
    value_ = new T();
    ::atexit(destroy); // 注册一个销毁函数
  }

  static void destroy()
  {
	// 这个技巧保证了类型完全 C语言不能声明-1 大小的数组 比如: class name;就是一个不完整的类型
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    delete value_;
  }

 private:
  static pthread_once_t ponce_;
  static T*             value_; // 静态的保证了内存中只有一份 即单例的
};

template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT; // must init static
// 这里是静态的全局声明的 ponce_ 需要PTHREAD_ONCE_INIT来初始化 成员变量要用init()函数来初始化

template<typename T>
T* Singleton<T>::value_ = NULL;			// must init static

}
#endif

