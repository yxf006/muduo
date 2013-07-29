#include <muduo/base/LogFile.h>
#include <muduo/base/Logging.h> // strerror_tl
#include <muduo/base/ProcessInfo.h>

#include <assert.h>
#include <stdio.h>
#include <time.h>

using namespace muduo;

// not thread safe
class LogFile::File : boost::noncopyable       		// 这里的class File是一个嵌套类
{
 public:
  explicit File(const string& filename)        		// 传入文件名 打开文件
    : fp_(::fopen(filename.data(), "ae")),
      writtenBytes_(0)
  {
    assert(fp_);
    ::setbuffer(fp_, buffer_, sizeof buffer_); 		// 设置文件缓冲区 保存了要写入文件的数据
    // posix_fadvise POSIX_FADV_DONTNEED ?
  }

  ~File()
  {
    ::fclose(fp_);                             		// 关闭文件
  }

  void append(const char* logline, const size_t len)// 添加日志 写入文件
  {
    size_t n = write(logline, len);
    size_t remain = len - n;
    while (remain > 0)
    {
      size_t x = write(logline + n, remain);
      if (x == 0)
      {
        int err = ferror(fp_);
        if (err)
        {
          fprintf(stderr, "LogFile::File::append() failed %s\n", strerror_tl(err));
        }
        break;
      }
      n += x; 			// 已经写入的个数
      remain = len - n; // remain -= x  剩余的个数
    }

    writtenBytes_ += len;
  }

  void flush()
  {
    ::fflush(fp_);
  }

  size_t writtenBytes() const { return writtenBytes_; }

 private:

  size_t write(const char* logline, size_t len)     // 写入日志内容到文件
  {
#undef fwrite_unlocked
    return ::fwrite_unlocked(logline, 1, len, fp_);
  }

  FILE* fp_;
  char buffer_[64*1024]; // 64K 如果缓存区大于这个会自动flush到文件中
  size_t writtenBytes_;
};

LogFile::LogFile(const string& basename,
                 size_t rollSize,
                 bool threadSafe,
                 int flushInterval)
  : basename_(basename),
    rollSize_(rollSize),
    flushInterval_(flushInterval),
    count_(0),
    mutex_(threadSafe ? new MutexLock : NULL),// 如果线程安全的需要构造一个MUtex对象
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0)
{
  assert(basename.find('/') == string::npos);// basename不包含/
  rollFile();
}

LogFile::~LogFile()
{
}

void LogFile::append(const char* logline, int len)
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    append_unlocked(logline, len);
  }
  else
  {
    append_unlocked(logline, len);
  }
}

void LogFile::flush()
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    file_->flush();
  }
  else
  {
    file_->flush();
  }
}

void LogFile::append_unlocked(const char* logline, int len) // 核心函数 无锁添加日志
{
  file_->append(logline, len);			 // 将这条日志写入当前日志文件

  if (file_->writtenBytes() > rollSize_) // 超过大小 滚动文件
  {
    rollFile();
  }
  else // 没有超过判断是否时间是零点是否是第二天的零点如果是 滚动文件
  {
    if (count_ > kCheckTimeRoll_) // 插入的记录是否超过1024 超过并且是第二天滚动日志文件
    {
      count_ = 0;
      time_t now = ::time(NULL);
      time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
      if (thisPeriod_ != startOfPeriod_) // 与开始的时间间隔不相同说明是第二天更新日志文件
      {
        rollFile();
      }
      else if (now - lastFlush_ > flushInterval_) // 距离上次写入的时间和时间间隔比较
      {
        lastFlush_ = now;
        file_->flush(); // 写入
      }
    }
    else
    {
      ++count_;
    }
  }
}

void LogFile::rollFile()
{
  time_t now = 0;
  string filename = getLogFileName(basename_, &now); // 获取文件名称和时间
  time_t start = now / kRollPerSeconds_ * kRollPerSeconds_; 

  // 对齐kRollPerSeconds_整数倍调整到当天的零点得到时间

  if (now > lastRoll_)     // 更新上次滚动的记录
  {
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = start;
    file_.reset(new File(filename));
  }
}

string LogFile::getLogFileName(const string& basename, time_t* now)
{
  string filename;
  filename.reserve(basename.size() + 64); // 日志文件名的长度
  filename = basename;

  char timebuf[32];
  char pidbuf[32];
  struct tm tm;
  *now = time(NULL);
  gmtime_r(now, &tm); // FIXME: localtime_r ? GMT(UTC)时间距离1970年的秒数
  // gmtime不是线程安全的 tm 对象保存了返回的时间
  
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm); // 将时间格式化
  filename += timebuf;
  filename += ProcessInfo::hostname();
  snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());
  filename += pidbuf; // 进程号
  filename += ".log"; // 再加上.log

  return filename;
}

