#include "Acceptor.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>

static int createNonblocking()
{
    int sockfd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if(sockfd < 0)
    {
        LOG_FATAL("%s:%s:%d listen socket create err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
    }
    return sockfd;
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
    : loop_(loop),
      acceptSocket_(createNonblocking()),       // 创建监听套接字
      acceptChannel_(loop, acceptSocket_.fd()), // 绑定Channel和socketfd
      listenning_(false)
{
    acceptSocket_.setReuseAddr(true);
    acceptSocket_.setReusePort(reuseport);
    acceptSocket_.bindAddress(listenAddr);      // bind

    acceptChannel_.setReadCallback(std::bind(&Acceptor::handleRead, this));     // 设置Channel的fd读回调函数
}

Acceptor::~Acceptor()
{
    acceptChannel_.disableAll();
    acceptChannel_.remove();
}

// TcpServer::start() 会调用此函数
void Acceptor::listen()
{
    listenning_ = true;
    acceptSocket_.listen();             // listen
    acceptChannel_.enableReading();     // 在Poller中关注可读事件
}

// listenfd 有事件发生 -> 有新用户连接 调用此函数 (事先被注册到 acceptChannel_ 的 ReadCallback中)
void Acceptor::handleRead()
{
    InetAddress peerAddr;
    int connfd = acceptSocket_.accept(&peerAddr);
    if(connfd >= 0)
    {
        if(NewConnectionCallback_)
        {
            // 轮询找到subLoop，唤醒，分发当前的新客户端的Channel
            NewConnectionCallback_(connfd, peerAddr);   
        }
        else
        {
            ::close(connfd);
        }
    }
    else
    {
        LOG_ERROR("%s:%s:%d accept err:%d\n", __FILE__, __FUNCTION__, __LINE__, errno);
        // 表示当前进程打开的文件描述符已达上限
        if(errno == EMFILE)
        {
            LOG_ERROR("%s:%s:%d sockfd reached limit!\n", __FILE__, __FUNCTION__, __LINE__);
            // muduo原本的处理方式是关闭事先创建的idleFd_，再去关闭accept让这个事件不会一直触发，不至于让LT模式产生坏的影响
        }    
    }
}