#pragma once
#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

/**
 *  Acceptor用于accept接受TCP连接
 *  数据成员主要包括Socket、Channel
 *  其中Socket是listenning socket，Channel用于观察此socket的可读事件，
 *  将handleRead()注册到读回调函数上(accept接收新连接，并且回调用户callback，将任务轮询分发到subloop中)
 *  不直接使用Acceptor类，而是将其封装作为TcpServer的成员
 */
class Acceptor : noncopyable
{
public:
    using NewConnectionCallback = std::function<void(int, const InetAddress&)>;
    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    {
        NewConnectionCallback_ = std::move(cb);
    }

    bool listenning() const { return listenning_; }
    void listen();

private:
    void handleRead();  // 可读回调函数

    EventLoop* loop_;   // Acceptor用的就是用户定义的那个baseLoop，也称作mainLoop
    Socket acceptSocket_;       // 监听套接字 
    Channel acceptChannel_;     // 和监听套接字绑定的Channel
    NewConnectionCallback NewConnectionCallback_;   // 一旦有新连接，就执行此回调函数
    bool listenning_;       //  acceptChannel所处的EventLoop是否处于监听状态
};