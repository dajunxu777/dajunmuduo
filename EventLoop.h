#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "noncopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;

//事件循环类    主要包含两个大模块 Channel  Poller (epoll的抽象)
/** EventLoop主要功能
 *  1. 首先我们调用updateChannel来添加一些事件(内部调用poller->updateChannel()来添加注册事件)
 *  2. 接着调用loop函数执行事件循环，在执行事件循环的过程中，在poller->poll()调用处，
 *     Poller类会把活跃的事件放在activeChannel集合中
 *  3. 然后调用Channel中的handleEvent来处理事件发生时对应的回调函数，
 *     处理完事件函数后还会处理必须由I/O线程来完成的doPendingFunctor函数
 */

class EventLoop : noncopyable
{
public:
    using Functor = std::function<void()>;
    EventLoop();
    ~EventLoop();

    // 开启事件循环
    void loop();
    // 退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    // 在当前loop中执行cb
    void runInLoop(Functor cb);
    // 把cb放入队列中，唤醒loop所在的线程，执行cb
    void queueInLoop(Functor cb);

    //用来唤醒loop所在的线程
    void wakeup();

    //供Channel中调用的接口，通过EventLoop调用Poller的方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    //判断EventLoop对象是否在自己的线程中
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
private:
    void handleRead(); //将事件通知描述符里的内容读走，以便让其继续检测事件通知
    void doPengingFunctors(); //执行回调    执行转交给I/O的任务

    using ChannelList = std::vector<Channel*>;  // 事件分发器列表

    std::atomic_bool looping_;  //是否运行  原子操作， 通过CAS实现
    std::atomic_bool quit_;     //标识退出loop循环

    const pid_t threadId_;      //记录当前loop所在线程id

    Timestamp pollReturnTime_;  //poller返回发生事件的channels的时间点 poll阻塞的时间

    std::unique_ptr<Poller> poller_;

    int wakeupFd_;  //当mainLoop获取一个新用户的channel，通过轮询算法选择一个subloop，通过wakeupFd_唤醒subloop处理channel
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;    //活跃的事件集

    std::atomic_bool callingPengingFunctors_;   //标识当前loop是否有需要执行的回调操作

    std::vector<Functor> pendingFunctors_;      //存储loop需要执行的所有回调操作
    
    std::mutex mutex_;  //互斥锁，用来保护上面vector容器的线程安全操作
};