#include <muduo/net/Connector.h>
#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>
#include <errno.h>

using namespace muduo;
using namespace muduo::net;

const int Connector::kMaxRetryDelayMs;

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
  : loop_(loop),
    serverAddr_(serverAddr),
    connect_(false),
    state_(kDisconnected),
    retryDelayMs_(kInitRetryDelayMs)
{
  LOG_DEBUG << "ctor[" << this << "]";
}

Connector::~Connector()
{
  LOG_DEBUG << "dtor[" << this << "]";
  assert(!channel_);
}

void Connector::start()
{
  connect_ = true;
  loop_->runInLoop(boost::bind(&Connector::startInLoop, this)); // FIXME: unsafe
}

void Connector::startInLoop()
{
  loop_->assertInLoopThread();
  assert(state_ == kDisconnected);
  if (connect_)
  {
    connect();
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

void Connector::stop()
{
  connect_ = false;
  loop_->runInLoop(boost::bind(&Connector::stopInLoop, this)); // FIXME: unsafe
  // FIXME: cancel timer
}

void Connector::stopInLoop()
{
  loop_->assertInLoopThread();
  if (state_ == kConnecting)
  {
    setState(kDisconnected);

	// 将通道从poller中移除关注 并且将channel置空
	// 这里并要重连 只是调用sockets::close(sockfd);
    int sockfd = removeAndResetChannel();
    retry(sockfd);
  }
}

void Connector::connect()
{
  int sockfd = sockets::createNonblockingOrDie(); // 创建非阻塞socket
  int ret = sockets::connect(sockfd, serverAddr_.getSockAddrInet()); // 调用sockets的connect()函数
  int savedErrno = (ret == 0) ? 0 : errno;
  switch (savedErrno)
  {
    case 0:
    case EINPROGRESS: // 非阻塞socket 未连接成功返回码EINPROGRESS表示正在连接
    case EINTR:
    case EISCONN:     // 连接成功
      connecting(sockfd);
      break;

    case EAGAIN:
    case EADDRINUSE:
    case EADDRNOTAVAIL:
    case ECONNREFUSED:
    case ENETUNREACH:
      retry(sockfd);
      break;

    case EACCES:
    case EPERM:
    case EAFNOSUPPORT:
    case EALREADY:
    case EBADF:
    case EFAULT:
    case ENOTSOCK:
      LOG_SYSERR << "connect error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);
      break;

    default:
      LOG_SYSERR << "Unexpected error in Connector::startInLoop " << savedErrno;
      sockets::close(sockfd);
      // connectErrorCallback_();
      break;
  }
}

void Connector::restart()
{
  loop_->assertInLoopThread();
  setState(kDisconnected);
  retryDelayMs_ = kInitRetryDelayMs;
  connect_ = true;
  startInLoop();
}

// 设置连接成功的读写处理函数
void Connector::connecting(int sockfd)
{
  setState(kConnecting);
  assert(!channel_);

  // channel与socket关联
  channel_.reset(new Channel(loop_, sockfd));
  channel_->setWriteCallback(
      boost::bind(&Connector::handleWrite, this)); // FIXME: unsafe
  channel_->setErrorCallback(
      boost::bind(&Connector::handleError, this)); // FIXME: unsafe

  // channel_->tie(shared_from_this()); is not working,
  // as channel_ is not managed by shared_ptr
  // 关注可写事件POLLOUT
  channel_->enableWriting();
}

int Connector::removeAndResetChannel()
{
  channel_->disableAll();
  channel_->remove();
  int sockfd = channel_->fd();
  // Can't reset channel_ here, because we are inside Channel::handleEvent
  // 不能在这里重置channel 因为正在调用handleEvent
  // 将函数加入到IO线程中等跳出了再执行这个函数
  loop_->queueInLoop(boost::bind(&Connector::resetChannel, this)); // FIXME: unsafe
  return sockfd;
}

void Connector::resetChannel()
{
  channel_.reset();
}

// 处理可写事件
void Connector::handleWrite()
{
  LOG_TRACE << "Connector::handleWrite " << state_;

  if (state_ == kConnecting)
  {
    int sockfd = removeAndResetChannel(); // 从poller中移除关注 并且将channel置空

	// socket可写并不意味着连接一定建立成功
	// 还需要使用getSocketError再次确认下
    int err = sockets::getSocketError(sockfd);
    if (err)
    {
      LOG_WARN << "Connector::handleWrite - SO_ERROR = "
               << err << " " << strerror_tl(err);
      retry(sockfd); // 有错误重连
    }
    else if (sockets::isSelfConnect(sockfd)) // 本地socket
    {
      LOG_WARN << "Connector::handleWrite - Self connect";
      retry(sockfd);
    }
    else // 连接成功
    {
      setState(kConnected);
      if (connect_)
      {
        newConnectionCallback_(sockfd);
      }
      else
      {
        sockets::close(sockfd);
      }
    }
  }
  else
  {
    // what happened?
    assert(state_ == kDisconnected);
  }
}

void Connector::handleError()
{
  LOG_ERROR << "Connector::handleError";
  assert(state_ == kConnecting);

  int sockfd = removeAndResetChannel();
  int err = sockets::getSocketError(sockfd);
  LOG_TRACE << "SO_ERROR = " << err << " " << strerror_tl(err);
  retry(sockfd);
}

// 采用back-off策略重连 即重连时间逐渐延长 0.5s 1s 2s ...一直至30s
void Connector::retry(int sockfd)
{
  sockets::close(sockfd);
  setState(kDisconnected);
  if (connect_)
  {
    LOG_INFO << "Connector::retry - Retry connecting to " << serverAddr_.toIpPort()
             << " in " << retryDelayMs_ << " milliseconds. ";

	// 注册一个定时操作 重连
    loop_->runAfter(retryDelayMs_/1000.0,
                    boost::bind(&Connector::startInLoop, shared_from_this()));
    retryDelayMs_ = std::min(retryDelayMs_ * 2, kMaxRetryDelayMs);
  }
  else
  {
    LOG_DEBUG << "do not connect";
  }
}

