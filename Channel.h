#pragma once

#include <functional>
#include "noncopyable.h"
#include "Timestamp.h"
#include <memory>
class EventLoop;

/*
    理清楚EventLoop、Channel、Poller之间的关系  <-> Reactor模型上对应 Demultiplex
    Channel理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN、EPOLLOUT事件
    还绑定了poller返回的具体事件

    主要作用：
    1. 首先我们给定Channel所属的loop以及要处理的fd
    2. 接着我们开始注册fd_上需要监听的事件，如果是常用事件(读写等)，可直接调用接口enable***来注册对应fd上的事件
        与之对应的是使用disable***来销毁特定的事件
    3. 再然后我们通过set***Callback来设置事件发生时的回调
*/

class Channel : noncopyable
{
public:
    using EventCallback = std::function<void()>;                //事件回调函数对象类型
    using ReadEventCallback = std::function<void(Timestamp)>;   //读事件回调函数对象类型

    Channel(EventLoop* loop, int fd);
    ~Channel();

    //  fd得到Poller通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);

    //  设置回调函数对象
    void setReadCallback(ReadEventCallback cb)
    { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb)
    { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb)
    { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb)
    { errorCallback_ = std::move(cb); }

    //防止Channel被手动remove掉， Channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);

    int fd() const { return fd_; }
    //返回注册的事件
    int events() const { return events_; }
    //设置就绪的事件
    void set_revents(int revt) { revents_ = revt; }

    //设置fd相应的事件状态
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; update(); }

    //判断事件是否被注册
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    //one loop per thread
    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;    //无事件
    static const int kReadEvent;    //可读事件
    static const int kWriteEvent;   //可写事件

    EventLoop *loop_;   //事件循环 Channel所属的loop
    const int fd_;      //fd, Poller监听的对象 
    int events_;        //注册fd感兴趣的事件
    int revents_;       //Poller返回的就绪的事件
    int index_;         //被Poller使用的下标

    std::weak_ptr<void> tie_;
    bool tied_;
    
    //因为Channel通道里面能够获知fd最终发生的具体的事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};