#pragma once

#include <vector>
#include <string>
#include <algorithm>


/**
 * prependable bytes：表示数据包的字节数
 * readerIndex：应用程序从readerIndex指向的位置开始读缓冲区，[readerIndex, writerIndex]表示待读取数据，
 * 读完后向后移动len（retrieve方法）
 * writerIndex：应用程序从writerIndex指向的位置开始写缓冲区，写完后writerIndex向后移动len（append方法）
 * [readerIndex_, writerIndex_]：标识可读数据区间（readableBytes方法） 
 */
class Buffer
{
public:
    // 通过预留 kCheapPrependable 空间，可以简化客户代码，一个简单的空间换时间思路
    static const size_t kCheapPrepend = 8;
    static const size_t kInitiaSize = 1024;

    explicit Buffer(size_t initialSize = kInitiaSize)
        : buffer_(kCheapPrepend + initialSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
    {}

    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    // 返回缓冲区可读数据的起始地址
    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    void retrieve(size_t len)
    {
        // len就是应用程序从Buffer缓冲区读取的数据长度
        // 必须要保证len <= readableBytes()
        if(len < readableBytes())
        {
            // 可读数据没有读完
            readerIndex_ += len;
        }
        else
        {
            // len == readableBytes()
            // 可读数据读完了，readerIndex_ 和 writerIndex_都要复位
            retrieveAll();
        }
    }


    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);    // peek返回缓冲区中可读数据的起始地址，从可读地址开始截取len个字符
        retrieve(len);                      // result保存了缓冲区中的可读数据，retrieve将参数复位
        return result;
    }

    // 把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes()); // 应用可读取数据的长度
    }

    void ensureWriteableBytes(size_t len)
    {
        if(writableBytes() < len)
        {
            // 扩容函数
            makeSpace(len);
        }
    }

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    // 不管是从fd上读数据写到缓冲区inputBuffer_，还是发数据要写入outputBuffer_，我们都要往writeable区间内添加数据
    // 把[data, data + len]内存上的数据，添加到writable缓冲区当中
    void append(const char* data, size_t len)
    {
        // 确保可写空间不小于len
        ensureWriteableBytes(len);
        // 把data中的数据往可写的位置writerIndex_写入
        std::copy(data, data + len, beginWrite());
        // len字节的data写入到Buffer后，就移动writerIndex_
        writerIndex_ += len;
    }

    // 从fd上读取数据，存放到writerIndex_，返回实际读取的数据大小
    ssize_t readFd(int fd, int* saveErrno);

    // 通过fd发送数据
    ssize_t writeFd(int fd, int* saveErrno);

private:
    char* begin()
    {
        // 返回vector底层数组首元素的地址，也就是数组的起始地址
        return &*buffer_.begin();
    }

    const char* begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        // 剩余空闲区间不够存储将要写入缓冲区的len数据了
        if(writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            // 直接在writerIndex_后面再扩大len的空间
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            // 如果是空闲空间足够存放len字节的数据，就把未读取的数据统一往前移，移到kCheapPrepend的位置
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};