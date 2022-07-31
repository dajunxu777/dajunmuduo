#pragma once
#include "noncopyable.h"
#include <unordered_map>
#include "Timestamp.h"
#include <vector>

class Channel;
class EventLoop;

//muduo库中多路事件分发器的核心IO复用模块

/*
    Poller主要功能：
    1. 调用poll函数监听注册了事件的文件描述符
    2. 当poll返回时将发生事件的事件集装入activeChannel中，并设置Channel发生事件到其revents_中
    3. 控制channel中事件的增删改
*/

class Poller : noncopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* loop);
    virtual ~Poller() = default;

    //给所有IO复用保留统一的接口
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0; //epoll_wait
    virtual void updateChannel(Channel* channel) = 0;                       //epoll_ctl
    virtual void removeChannel(Channel* channel) = 0;                       //epoll_ctl

    //判断参数channel是否在当前Poller当中
    bool hasChannel(Channel* channel) const;

    //EventLoop可以通过该接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop* loop);
protected:
    //map的key : sockfd      val : sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int, Channel*>;
    ChannelMap channels_; 
private:
    EventLoop* ownerLoop_;  //定义Poller所属的事件循环EventLoop
};


