//
//  BufferWriteInListCs.hpp
//  KEXTLogKit
//
//  Created by Angus on 2026/5/16.
//

#ifndef BufferWriteInListCs_hpp
#define BufferWriteInListCs_hpp

#include <libkern/OSMalloc.h>
#include "TempBufferCs.hpp"
#include "TaskLock.hpp"

/**
 * ------------------------------------------------------------------------
 * BufferNode
 *
 * 内部链表节点，用于 BufferWriteInListCs 队列。
 * 每个节点持有一个 TempBuffer* 指针和下一个节点指针。
 * 内核态手动管理内存，无 STL。
 * ------------------------------------------------------------------------
 */
struct BufferNode
{
    TempBuffer* buffer;  // 指向存放的临时缓冲区
    BufferNode* next;    // 下一个节点
    BufferNode(TempBuffer* buf) : buffer(buf), next(nullptr) {}
};

/**
 * ------------------------------------------------------------------------
 * BufferWriteInListCs
 *
 * 内核态队列类（链表实现，无 STL），用于临时存放日志数据。
 *
 * 特点：
 *   - 内核态安全，内部使用 TaskLock 上锁
 *   - 支持移动语义，但禁止拷贝
 *   - 手动管理节点和 TempBuffer 内存
 *
 * 使用方法：
 *   BufferWriteInListCs queue;
 *   queue.pushlist(buf);   // 入队
 *   TempBuffer* t = queue.dequeue(); // 出队
 *   size_t len = queue.size();       // 队列长度
 *   queue.clear();                   // 清空队列
 *
 * 注意：
 *   1. TempBuffer 必须使用 IOMalloc 分配，否则 clear() 释放时会出错
 *   2. 不支持多线程同时写入同一个队列，需在外层加锁
 * ------------------------------------------------------------------------
 */
class BufferWriteInListCs
{
private:
    BufferNode* head;   // 队列头
    BufferNode* tail;   // 队列尾
    size_t listSize;    // 当前队列长度
    TaskLock lock;      // 内核锁保护队列操作

public:
    // ---------------------------------------------------------------------
    // 构造函数：初始化空队列
    // ---------------------------------------------------------------------
    BufferWriteInListCs();

    // ---------------------------------------------------------------------
    // 析构函数：释放所有节点和 TempBuffer
    // ---------------------------------------------------------------------
    ~BufferWriteInListCs();

    // ---------------------------------------------------------------------
    // 禁止拷贝
    // ---------------------------------------------------------------------
    BufferWriteInListCs(const BufferWriteInListCs&) = delete;
    BufferWriteInListCs& operator=(const BufferWriteInListCs&) = delete;

    // ---------------------------------------------------------------------
    // 支持移动语义（移动构造和移动赋值）
    // ---------------------------------------------------------------------
    BufferWriteInListCs(BufferWriteInListCs&& other) noexcept;
    BufferWriteInListCs& operator=(BufferWriteInListCs&& other) noexcept;

    // ---------------------------------------------------------------------
    // 入队
    // @param buffer: 已分配的 TempBuffer*（必须通过 IOMalloc）
    // ---------------------------------------------------------------------
    void pushlist(TempBuffer* buffer);

    // ---------------------------------------------------------------------
    // 出队
    // @return TempBuffer* 指针，如果队列为空返回 nullptr
    // ---------------------------------------------------------------------
    TempBuffer* dequeue();

    // ---------------------------------------------------------------------
    // 获取当前队列大小
    // @return 队列中元素个数
    // ---------------------------------------------------------------------
    size_t size();

    // ---------------------------------------------------------------------
    // 清空队列
    // 会释放所有 BufferNode 和 TempBuffer
    // ---------------------------------------------------------------------
    void clear();

    // ---------------------------------------------------------------------
    // 遍历队列
    // @param callback: 回调函数，每个 TempBuffer* 会调用一次
    // ---------------------------------------------------------------------
    void forEach(void (*callback)(TempBuffer*));
};

#endif /* BufferWriteInListCs_hpp */
