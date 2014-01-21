#ifndef MUDUO_BASE_LOGFILE_H
#define MUDUO_BASE_LOGFILE_H

#include <muduo/base/Mutex.h>
#include <muduo/base/Types.h>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{

class LogFile : boost::noncopyable
{
 public:
  LogFile(const string& basename,
          size_t rollSize,
          bool threadSafe = true,
          int flushInterval = 3);
  ~LogFile();

  void append(const char* logline, int len); // 添加日志
  void flush(); // 写入文件

 private:
  void append_unlocked(const char* logline, int len);

  static string getLogFileName(const string& basename, time_t* now);
  void rollFile(); // 滚动文件

  const string basename_; 						// basename(filename)
  const size_t rollSize_; 						// 达到这个大小时创建新的文件
  const int flushInterval_;						// 日志写入间隔时间

  int count_;

  boost::scoped_ptr<MutexLock> mutex_;  
  
  time_t startOfPeriod_;  // 开始记录日志时间(调整至零点的时间) 这一天距离1971.9.1的时间是一样的
  time_t lastRoll_;      						// 上一次滚动日志文件时间
  time_t lastFlush_;	 						// 上一次写入日志文件时间
  class File;
  boost::scoped_ptr<File> file_; // 子类File

  const static int kCheckTimeRoll_ = 1024;      // 如果count_计数器大小达到这个值就滚动
  const static int kRollPerSeconds_ = 60*60*24; // 每一天滚动 新的一天 每多少秒滚动
};

}
#endif  // MUDUO_BASE_LOGFILE_H
