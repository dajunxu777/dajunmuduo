#pragma once
#include "noncopyable.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>

class EventLoop;
class EventLoopThread;

/**
 *      线程池类 EventLoopThreadPool
 *  由1个主线程+N个子线程组成，“1+N”的模式一般用在服务端，主线程用于接收所有客户端的连接请求，
 *  各子线程用于处理多个客户端的IO事件。给线程池分配客户端的时候采取循环分配的方式，保证线程池的负载平衡。
 *  EventLoopThread作为EventLoopThreadPool的成员对象，
 *  多个EventLoopThread对象保存到std::vector中构成了EventLoopThreadPool的线程池。
 */

class EventLoopThreadPool : noncopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    EventLoopThreadPool(EventLoop* baseLoop, const std::string& nameArg);
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }

    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    // 如果工作在多线程中，baseLoop_默认以轮询的方式分配channel给subloop
    EventLoop* getNextLoop();

    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }
    const std::string name() const { return name_; }

private:
    EventLoop* baseLoop_;
    std::string name_;      // 名字
    bool started_;          // 是否开始，在start()函数中被赋值为true
    int numThreads_;        // 线程数量
    int next_;              // 新连接到来时，所选择的EventLoop对象下标
    std::vector<std::unique_ptr<EventLoopThread>> threads_;     // IO线程列表
    std::vector<EventLoop*> loops_;     // EventLoop列表
};