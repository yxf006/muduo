#ifndef MUDUO_BASE_ATOMIC_H
#define MUDUO_BASE_ATOMIC_H

#include <boost/noncopyable.hpp>
#include <stdint.h>

namespace muduo
{

namespace detail
{
template<typename T>
class AtomicIntegerT : boost::noncopyable
{
 public:
  AtomicIntegerT()
    : value_(0)
  {
  }

  T get()
  {
    return __sync_val_compare_and_swap(&value_, 0, 0);/* 原子性操作 是线程安全的 */
  }

  T getAndAdd(T x)
  {
    return __sync_fetch_and_add(&value_, x); // 初始化value_为0 所以这里加后结果就是x
  }

  T addAndGet(T x)
  {
    return getAndAdd(x) + x;
  }

  T incrementAndGet()
  {
    return addAndGet(1);
  }

  T decrementAndGet()
  {
    return addAndGet(-1);
  }

  void add(T x)
  {
    getAndAdd(x);
  }

  void increment()
  {
    incrementAndGet();
  }

  void decrement()
  {
    decrementAndGet();
  }

  T getAndSet(T newValue)
  {
    return __sync_lock_test_and_set(&value_, newValue);
  }

 private:
  volatile T value_;
};
}

typedef detail::AtomicIntegerT<int32_t> AtomicInt32; // 32位整数的实例化
typedef detail::AtomicIntegerT<int64_t> AtomicInt64; // 64位整数的实例化
}

#endif  // MUDUO_BASE_ATOMIC_H
