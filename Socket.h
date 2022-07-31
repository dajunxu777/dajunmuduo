#pragma once
#include "noncopyable.h"

class InetAddress;

/**
 *  Socket类其实就是封装了一个sockfd，实现了bind()、listen()、accept()等函数功能，还可以设置sockfd的各种属性
 * 
 */
class Socket : noncopyable
{
public:
    explicit Socket(int sockfd) : sockfd_(sockfd)
    {}

    ~Socket();

    int fd() const { return sockfd_; }
    void bindAddress(const InetAddress& localaddr);
    void listen();
    int accept(InetAddress* peeraddr);

    void shutdownWrite();

    void setTcpNoDelay(bool on);
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setKeepAlive(bool on);

private:
    const int sockfd_;
};