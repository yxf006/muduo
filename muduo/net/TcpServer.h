#ifndef MUDUO_NET_TCPSERVER_H
#define MUDUO_NET_TCPSERVER_H

#include <muduo/base/Types.h>
#include <muduo/net/TcpConnection.h>

#include <map>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{
namespace net
{

class Acceptor;
class EventLoop;
class EventLoopThreadPool;

/// TCP server, supports single-threaded and thread-pool models.
class TcpServer : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  //TcpServer(EventLoop* loop, const InetAddress& listenAddr);
  TcpServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg);
  ~TcpServer();  // force out-line dtor, for scoped_ptr members.

  const string& hostport() const { return hostport_; }
  const string& name() const { return name_; }

  void setThreadNum(int numThreads);
  void setThreadInitCallback(const ThreadInitCallback& cb)
  { threadInitCallback_ = cb; }

  /// It's harmless to call it multiple times.
  /// Thread safe.
  void start();

  /// Set connection callback.
  /// Not thread safe.
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  /// Set message callback.
  /// Not thread safe.
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  /// Set write complete callback.
  /// Not thread safe.
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

 private:
  /// Not thread safe, but in loop
  void newConnection(int sockfd, const InetAddress& peerAddr);
  /// Thread safe.
  void removeConnection(const TcpConnectionPtr& conn);
  /// Not thread safe, but in loop
  void removeConnectionInLoop(const TcpConnectionPtr& conn);

  // typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr;
  typedef std::map<string, TcpConnectionPtr> ConnectionMap; // 客户端连接列表map

  EventLoop* loop_;        // the acceptor loop
  const string hostport_;
  const string name_;
  
  boost::scoped_ptr<Acceptor> acceptor_; // avoid revealing Acceptor
  
  boost::scoped_ptr<EventLoopThreadPool> threadPool_; // EventLoopPool池
  ConnectionCallback    connectionCallback_; // 读写回调函数 。。。
  MessageCallback       messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  ThreadInitCallback    threadInitCallback_;
  bool started_;
  // always in loop thread
  int nextConnId_;            // 下一个连接id
  ConnectionMap connections_; // 连接列表map
};

}
}

#endif  // MUDUO_NET_TCPSERVER_H
