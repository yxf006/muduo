#include <muduo/base/LogFile.h>
#include <muduo/base/Logging.h>

boost::scoped_ptr<muduo::LogFile> g_logFile;

void outputFunc(const char* msg, int len)
{
  g_logFile->append(msg, len);
}

void flushFunc()
{
  g_logFile->flush();
}

int main(int argc, char* argv[])
{
  char name[256];
  strncpy(name, argv[0], 256);
  g_logFile.reset(new muduo::LogFile(::basename(name), 200*1000));

  
  // 我们只要在这里设置Logger的输出函数为文件即可将日志写入到文件中
  muduo::Logger::setOutput(outputFunc); 
  muduo::Logger::setFlush(flushFunc);

  muduo::string line = "1234567890 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

  for (int i = 0; i < 10000; ++i)
  {
    LOG_INFO << line << i;

    usleep(1000);
  }
}
