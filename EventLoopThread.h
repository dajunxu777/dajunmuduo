#pragma once

#include "noncopyable.h"
#include "Thread.h"

#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

class EventLoop;

/**
 *  IO线程类
 *  muduo用EventLoopThread提供了对应EventLoop和Thread的封装，EventLoopThread可以创建一个IO线程，
 *  通过startLoop返回一个IO线程的loop，threadFunc中开启loop循环
 */
class EventLoopThread
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
        const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};


