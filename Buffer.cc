#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

/**
 *  从fd上读取数据  Poller工作在LT模式
 *  Buffer缓冲区是有大小的！ 但是从fd上读数据的时候，却不知道tcp数据最终的大小
 */

/**
 * 这里有个小技巧，就是巧妙运用了栈的空间，如果读取的数据不多，没有超过Buffer的长度，那么直接读取到Buffer里，
 * 如果大于Buffer的可写长度，那么我们就先用栈保存起来， 这样不用把Buffer的长度设置的太大，免得避免浪费，
 * 只有在Buffer的长度不够用时，才会进行扩容。然后将栈里数据追加到Buffer里，最后这个局部变量栈空间在函数结束时，就自动释放了。
 * 保证每次至少读64K数据，减少了系统调用，同时使用栈空间而不是堆空间减少了内存占用
 * 使用了65536字节的栈空间，开辟效率高，回收快，用于存储Buffer对象无法存储的，剩余的数据
 * 
 * 我们给readv两个缓冲区，第一个就是Buffer对象的空间（一般是堆空间），第二个是65536字节的栈空间。
 * readv会先写入第一个缓冲区，没写完再写入第二个缓冲区。
 * 如果读取了65536字节数据，fd上的数据还是没有读完，那就等Poller下一次上报（工作在LT模式），继续读取，数据不会丢失
 */
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    // 巧妙地利用栈空间， 减少了系统调用和内存占用
    char extrabuf[65536] = {0};     // 栈空间 64K

    struct iovec vec[2];

    const size_t writable = writableBytes();    // Buffer底层缓冲区剩余的可写空间大小
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if(n < 0)
    {
        *saveErrno = errno;
    }
    else if(n <= writable)  // Buffer的可写缓冲区已经够存储读出来的数据了
    {
        writerIndex_ += n;
    }
    else // extrabuf里面也写入了数据
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);  // writerIndex_开始写 n - writable大小的数据, append会调用makeSpace扩容
    }
    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}