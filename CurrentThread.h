#pragma once

#include <unistd.h>
#include <sys/syscall.h>

namespace CurrentThread
{
    /*
        __thread是GCC内置的线程局部存储设施。thread_local
        __thread变量每一个线程有一份独立实体，各个线程的值互不干扰。
        用于存储线程ID
    */
    extern __thread int t_cachedTid;

    void cachedTid();

    //由于系统调用(syscall(SYS_gettid))会陷入内核，频繁系统调用可能会影响系统性能
    inline int tid()
    {
        /*
            __builtin_expect 分支预测优化指令   __builtin_expect（EXP, N）说明 EXP == N的概率很大
            if(__builtin_expect(t_cachedTid == 0, 0))   
            等价于 if(t_cachedTid == 0) 
        */
        if(__builtin_expect(t_cachedTid == 0, 0))
        {
            cachedTid();
        }
        return t_cachedTid;
    }
}