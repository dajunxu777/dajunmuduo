#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection Loop is null! \n", __FILE__, __FUNCTION__, __LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop,
                            const std::string& nameArg,
                            int sockfd,
                            const InetAddress& localAddr,
                            const InetAddress& peerAddr)
        : loop_(CheckLoopNotNull(loop)),
          name_(nameArg),
          state_(kConnecting),
          reading_(true),
          socket_(new Socket(sockfd)),
          channel_(new Channel(loop, sockfd)),
          localAddr_(localAddr),
          peerAddr_(peerAddr),
          highWaterMark_(64*1024*1024)  // 64M
{
    // 给channel设置相应的回调函数，当poller通知channel感兴趣的事件发生之后，channel会回调相应的操作函数
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO("TcpConnection::ctor[%s] at fd = %d\n", name_.c_str(), sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd = %d state = %d\n", name_.c_str(), channel_->fd(), (int)state_);
}

void TcpConnection::send(const std::string& buf)
{
    // 处于连接状态才发送
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            // 如果是当前线程就直接发送
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            //如果Loop在别的线程中这放到loop待执行回调队列执行
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

/**
 *  发送数据    应用写的快，而内核发送数据慢，需要把待发送数据写入缓冲区，并且设置了水位回调
 */
void TcpConnection::sendInLoop(const void* message, size_t len)
{
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool faultError = false;

    // 之前调用过该connection的shutdown，不能再进行发送了
    if(state_ == kDisconnected)
    {
        LOG_ERROR("disconnected, give up writing!");
        return;
    }

    //如果通道没在写数据，同时输出缓存是空的
    //则直接往fd中写数据，即发送
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), message, len);
        if(nwrote >= 0)
        {
            //发送数据 >= 0
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                //若数据一次性都发完了，同时也设置了写完成回调。
	            //则调用下写完成回调函数。
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else    // nwrote < 0
        {
            //如果写返回值小于0，表示写出错
	        //则把已写数据大小设置为0。
            nwrote = 0;
            if(errno != EWOULDBLOCK)
            {
                LOG_ERROR("TcpConnection::sendInLoop err");
                if(errno == EPIPE || errno == ECONNRESET)   //  SIGPIPE RESET
                {
                    faultError = true;
                }
            }
        }
    }

    // 如果还有残留的数据没有发送完成
    if(remaining > 0)
    {
        size_t oldlen = outputBuffer_.readableBytes();
        if(oldlen + remaining >= highWaterMark_ && oldlen < highWaterMark_ && highWaterMarkCallback_)
        {
            //添加新的待发送数据之后，如果数据大小已超过设置的警戒线
	        //则回调下设置的高水平阀值回调函数，对现有的长度做出处理。
	        //高水平水位线的使用场景?
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldlen + remaining));
        }
        // 往outputBuffer后面添加数据
        outputBuffer_.append(static_cast<const char*>(message) + nwrote, remaining);
        if(!channel_->isWriting())
        {
            //将通道置成可写状态。这样当通道活跃时，
	        //就会调用TcpConnection的可写方法。
	        //对实时要求高的数据，这种处理方法可能有一定的延时。

            // 当可写事件被触发，就可以继续发送了，调用的是TcpConnection::handleWrite()
            channel_->enableWriting();
        }
    }

}

//关闭动作，如果状态是连接，则要调用下关闭动作。
void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

/**
 *  a、通道不再写数据，则直接关闭写
 *  b、通道若处于写数据状态，则不做处理，留给后面处理。
 *  后面是指在handleWrite的时候，如果发现状态是断开，则调用shutdownWrite
 */
void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting()){     // 说明outputBuffer中的数据已经全部发送完成
        socket_->shutdownWrite();   // 关闭写端
    }
}

/**
 *  连接建立完成方法，当TcpServer accepts a new connection时，调用此方法
 *  a、设置kConnected状态
 *  b、调用channel的tie方法，并设置可读
 *  c、调用连接建立完成的回调函数
 */
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 向poller注册channel的epollin事件, 最终调用epoll_ctl

    // 连接成功，回调客户注册的函数（由用户提供的函数，比如OnConnection）
    connectionCallback_(shared_from_this());
}

/**
 *  连接销毁。当TcpServer将TcpConnection从map列表中清除时，会调用此方法。
 *  a、设置kDisconnected状态
 *  b、关闭chanel
 *  c、调用连接回调函数(用户设置)
 *  d、移除channel
 */
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

/**
 *  当某个channel有读事件发生时，会调用TcpConnection::handleRead()函数，
 *  然后从socket里读入数据到buffer，再通过回调把这些数据返回给用户层。
 */
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int saveErrno = 0;
    // 读数据到inputBuffer_中
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &saveErrno);
    if(n > 0)
    {
        // 已建立连接的用户，有可读事件发生了，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    // 读到了0，表明客户端已经关闭了
    else if(n == 0)
    {
        handleClose();
    }
    else
    {
        errno = saveErrno;
        LOG_ERROR("TcpConnection::handleRead err");
        handleError();
    }
}

/**
 *  当可写事件发生时调用TcpConnection::handleWrite()
 */
void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int saveErrno = 0;
        // 写数据
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &saveErrno);
        if(n > 0)
        {
            // 调整发送buffer的内部index，以便下次继续发送
            outputBuffer_.retrieve(n);
            // 如果对于系统发送函数来说，可读的数据量为0，表示所有数据都被发送完毕了，即写完成了
            if(outputBuffer_.readableBytes() == 0)
            {
                // 不再关注写事件
                channel_->disableWriting();
                if(writeCompleteCallback_)
                {
                    // 唤醒loop_对应的thread线程，执行回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                // 如果当前状态是正在关闭连接，那么就调用shutdown来主动关闭连接
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite err");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd = %d is down, no more writing\n", channel_->fd());
    }
}

/**
 *  当对端调用shutdown()关闭连接时，本端会收到一个FIN，channel的读事件被触发，但inputBuffer_.readFd() 会返回0，然后调用
 *  handleClose()，处理关闭事件，最后调用TcpServer::removeConnection()。
 *  这里fd不关闭，fd是外部传入的，当TcpConnection析构时，Sockets会析构，由Sockets去关闭socket
 */
void TcpConnection::handleClose()
{
    LOG_INFO("TcpConnection::handleClose fd = %d, state = %d\n", channel_->fd(), (int)state_);
    setState(kDisconnected);
    // channel上不再关注任何事情
    channel_->disableAll();
    // 获得shared_ptr交由tcpsever处理
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);   // 执行关闭连接的回调
    closeCallback_(connPtr);        // 关闭连接的回调   执行的是TcpServer::removeConnection回调方法
}

// 处理出错事件
void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    // getsockopt能够通过SO_ERROR得到正确的错误码
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d\n", name_.c_str(), err);
}
