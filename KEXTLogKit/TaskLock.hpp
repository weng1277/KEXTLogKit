//
// TaskLock.hpp
// 内核线程安全锁（RAII）
//

#ifndef TaskLock_hpp
#define TaskLock_hpp

#include <IOKit/IOLib.h>

/**
 * TaskLock
 * 内核态互斥锁封装类
 *
 * 使用方式：
 *   1. 定义锁对象
 *        TaskLock myLock;
 *
 *   2. 手动加锁/解锁
 *        myLock.lockAcquire();
 *        // 临界区
 *        myLock.lockRelease();
 *
 *   3. RAII 自动加锁
 *        {
 *            TaskLock::Guard guard(myLock);
 *            // 临界区，离开作用域自动解锁
 *        }
 *
 * 线程安全示例：
 *   TaskLock lock;
 *   int sharedVar = 0;
 *
 *   void increment()
 *   {
 *       TaskLock::Guard g(lock);
 *       sharedVar++; // 安全
 *   }
 */
class TaskLock
{
private:
    IOLock* lock = nullptr;

public:
    TaskLock();               // 分配内核锁
    ~TaskLock();              // 释放锁

    void lockAcquire();       // 上锁
    void lockRelease();       // 解锁

    // -------------------
    // RAII Guard 类
    // -------------------
    class Guard
    {
    private:
        TaskLock& targetLock;

    public:
        explicit Guard(TaskLock& l);   // 构造函数自动加锁
        ~Guard();                       // 析构函数自动解锁
    };
};

#endif /* TaskLock_hpp */
