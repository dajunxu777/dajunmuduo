#pragma once

#include "noncopyable.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"

#include <functional>
#include <string>
#include <memory>
#include <atomic>
#include <unordered_map>

// 对外的服务器编程使用的类

/**
 *  这是一个接口类，拥有一个管理监听套接字的类acceptor，拥有一张具有多个管理连接套接字的TcpConnection类的映射表
 *  它对这两个类进行管理，会设置它们的一些回调函数，监听端口等，负责acceptor和TcpConnection两个类与用户交互的接口，
 *  而具体的调用实现还是由那两个类去实现。
 */
class TcpServer : noncopyable
{
public:
    // 线程初始化函数，并不一定需要
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    enum Option
    {
        kNoReusePort,
        kReusePort
    };

    // TcpServer(EventLoop* loop, const InetAddress& listenAddr);
    TcpServer(EventLoop* loop,
                const InetAddress& listenAddr,
                const std::string& nameArg,
                Option option = kNoReusePort);

    ~TcpServer();
    
    // 设置线程初始化函数
    void setThreadInitcallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
    // 设置建立连接成功后的回调，不是线程安全的
    void setConnectionCallback(const ConnectionCallback& cb) { connectionCallback_ = cb; }
    // 设置消息回调，不是线程安全的
    void setMessageCallback(const MessageCallback& cb) { messageCallback_ = cb; }
    // 设置写完成回调，不是线程安全的
    void setWriteCompleteCallback(const WriteCompleteCallback& cb) { writeCompleteCallback_ = cb; }
    
    // 设置底层subloop的个数
    /*  设置I/O线程池中线程的数量,一定在start函数前调用
        这些线程总是接收新的连接
        参数为0时：没有I/O线程被建立，参数为0时默认值
        参数为N时：有多个线程，新的连接被 基于轮询的I/O线程池分配。
    */
    void setThreadNum(int numThreads);

    // 开启服务器监听
    void start();

private:
    // acceptor设置的回调
    void newConnection(int sockfd, const InetAddress& peerAddr);
    // 移除已建立的连接
    void removeConnection(const TcpConnectionPtr& conn);
    // removeConnection中调用的连接
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    // 记录已建立连接TcpConnectionPtr的hash表
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    EventLoop* loop_;   // baseLoop 用户定义的loop  the acceptor loop

    const std::string ipPort_;  //本地地址
    const std::string name_;    //服务名字

    std::unique_ptr<Acceptor> acceptor_;    // 运行在mainLoop，任务就是监听新连接事件

    std::shared_ptr<EventLoopThreadPool> threadPool_;   // one loop per thread

    ConnectionCallback connectionCallback_; // 有新连接时的回调
    MessageCallback messageCallback_;       // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_; // 消息发送完成以后的回调
    
    ThreadInitCallback threadInitCallback_; // loop线程初始化的回调 

    std::atomic_int started_;   // started_变量，调用start方法后+1，防止一个TcpServer对象被start多次

    int nextConnId_;
    ConnectionMap connections_;     // 保存所有的连接

};