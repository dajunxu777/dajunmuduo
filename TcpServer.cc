#include "TcpServer.h"
#include "Logger.h"
#include <strings.h>
#include <functional>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpServer::TcpServer(EventLoop* loop,
                const InetAddress& listenAddr,
                const std::string& nameArg,
                Option option)
                : loop_(CheckLoopNotNull(loop)),
                  ipPort_(listenAddr.toIpPort()),
                  name_(nameArg),
                  acceptor_(new Acceptor(loop, listenAddr, option == kNoReusePort)),
                  threadPool_(new EventLoopThreadPool(loop, name_)),
                  connectionCallback_(),
                  messageCallback_(),
                  nextConnId_(1),
                  started_(0)
{
    // 当有新用户连接时，会执行TcpServer::newConnection回调
    // 把newConnection设置为acceptor的回调函数
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection,
                                        this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer()
{
    for(auto& item : connections_)
    {
        // 这是个栈上的智能指针对象，出作用域会自动释放new出来的TcpConnection对象资源
        TcpConnectionPtr conn(item.second);
        item.second.reset();    // reset()将引用计数减1

        // 销毁连接
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

// 设置底层subloop的个数
void TcpServer::setThreadNum(int numThreads)
{
    threadPool_->setThreadNum(numThreads);
}

// 开启服务器监听
void TcpServer::start()
{
    if(started_++ == 0) // 防止一个TcpServer对象被start多次
    {
        threadPool_->start(threadInitCallback_);    // 启动底层的loop线程池
        loop_->runInLoop(std::bind(&Acceptor::listen, acceptor_.get()));
    }
}

// 有一个新的客户端连接，acceptor会执行这个回调操作
/**
 *  首先获取创建TcpConnection需要的信息，然后通过这些信息new一个TcpConnection对象，
 *  使用智能指针TcpConnectionPtr管理TcpConnection对象资源，然后将该TcpConnectionPtr
 *  添加到哈希表中进行管理。
 *  对新的TcpConnection设置对应的回调函数 (用户传入)
 *  TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调
 */
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
    // 轮询算法，选择一个subLoop，来管理channel
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[64] = {0};
    snprintf(buf, sizeof buf, "-%s$%d", ipPort_.c_str(), nextConnId_);
    ++nextConnId_;
    std::string connName = name_ + buf;
    LOG_INFO("TcpServer::newConnection [%s] - new connection [%s] from %s\n",
            name_.c_str(), connName.c_str(), peerAddr.toIpPort().c_str());
    // 通过sockfd获取其绑定的本机ip地址和端口信息
    sockaddr_in local;
    ::bzero(&local, sizeof local);
    socklen_t addrlen = sizeof local; 
    if(::getsockname(sockfd, (sockaddr*)&local, &addrlen) < 0)
    {
        LOG_ERROR("socket::getLocalAddr");
    }

    InetAddress localAddr(local);

    // 根据连接成功的sockfd，创建TcpConnection连接对象
    TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    // 将刚生成的TcpConnectionPtr添加到哈希表中管理
    connections_[connName] = conn;
    
    // 下面的回调都是用户设置 TcpServer=>TcpConnection=>Channel=>Poller=>notify channel调用回调
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    conn->setWriteCompleteCallback(writeCompleteCallback_);

    // 设置了如何关闭连接的回调
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));

    // 直接调用TcpConnection::connectEstablished
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));

}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
    LOG_INFO("TcpServer::removeConnectionInLoop [%s] - connection %s\n", name_.c_str(), conn->name().c_str());
    connections_.erase(conn->name());
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
