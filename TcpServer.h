#pragma once 
 
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "noncopyable.h"
#include "EventLoopThreadPoll.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// 对外的服务器编程使用的类
class TcpServer:noncopyable
{
public:
    using ThreadInitCallback=std::function<void(EventLoop*)>;

    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

private:
    void newConnection(int sockfd,const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void 

};