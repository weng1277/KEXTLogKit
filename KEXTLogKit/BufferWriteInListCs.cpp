#include "BufferWriteInListCs.hpp"
#include <IOKit/IOLib.h>

// -----------------------------------------------------------------------------
// 构造函数
// -----------------------------------------------------------------------------
BufferWriteInListCs::BufferWriteInListCs()
    : head(nullptr), tail(nullptr), listSize(0)
{
}

// -----------------------------------------------------------------------------
// 移动构造
// -----------------------------------------------------------------------------
BufferWriteInListCs::BufferWriteInListCs(BufferWriteInListCs&& other) noexcept
    : head(nullptr), tail(nullptr), listSize(0)
{
    TaskLock::Guard guard(const_cast<TaskLock&>(other.lock));
    head = other.head;
    tail = other.tail;
    listSize = other.listSize;

    other.head = nullptr;
    other.tail = nullptr;
    other.listSize = 0;
}

// -----------------------------------------------------------------------------
// 移动赋值
// -----------------------------------------------------------------------------
BufferWriteInListCs& BufferWriteInListCs::operator=(BufferWriteInListCs&& other) noexcept
{
    if (this != &other)
    {
        TaskLock::Guard guardThis(lock);
        TaskLock::Guard guardOther(const_cast<TaskLock&>(other.lock));

        clear();

        head = other.head;
        tail = other.tail;
        listSize = other.listSize;

        other.head = nullptr;
        other.tail = nullptr;
        other.listSize = 0;
    }
    return *this;
}

// -----------------------------------------------------------------------------
// 析构函数
// -----------------------------------------------------------------------------
BufferWriteInListCs::~BufferWriteInListCs()
{
    clear();
}

// -----------------------------------------------------------------------------
// 入队
// -----------------------------------------------------------------------------
void BufferWriteInListCs::pushlist(TempBuffer* buffer)
{
    if (!buffer) return;

    // 分配 BufferNode 内存
    BufferNode* node = (BufferNode*)IOMalloc(sizeof(BufferNode));
    if (!node) return;

    // 内核态直接手动初始化结构体
    node->buffer = buffer;
    node->next = nullptr;

    TaskLock::Guard guard(lock);

    if (!tail)
    {
        head = tail = node;
    }
    else
    {
        tail->next = node;
        tail = node;
    }

    listSize++;
}

// -----------------------------------------------------------------------------
// 出队
// -----------------------------------------------------------------------------
TempBuffer* BufferWriteInListCs::dequeue()
{
    TaskLock::Guard guard(lock);

    if (!head) return nullptr;

    BufferNode* node = head;
    TempBuffer* buf = node->buffer;

    head = node->next;
    if (!head)
        tail = nullptr;

    listSize--;

    // 释放 BufferNode 内存
    IOFree(node, sizeof(BufferNode));

    return buf;
}

// -----------------------------------------------------------------------------
// 获取队列大小
// -----------------------------------------------------------------------------
size_t BufferWriteInListCs::size()
{
    TaskLock::Guard guard(lock);
    return listSize;
}

// -----------------------------------------------------------------------------
// 清空队列
// -----------------------------------------------------------------------------
void BufferWriteInListCs::clear()
{
    TaskLock::Guard guard(lock);

    BufferNode* node = head;
    while (node)
    {
        BufferNode* next = node->next;
        if (node->buffer)
            IOFree(node->buffer, sizeof(TempBuffer)); // 假设 TempBuffer 也用 IOMalloc

        IOFree(node, sizeof(BufferNode));
        node = next;
    }

    head = tail = nullptr;
    listSize = 0;
}

// -----------------------------------------------------------------------------
// 遍历队列
// -----------------------------------------------------------------------------
void BufferWriteInListCs::forEach(void (*callback)(TempBuffer*))
{
    if (!callback) return;

    TaskLock::Guard guard(lock);

    BufferNode* node = head;
    while (node)
    {
        callback(node->buffer);
        node = node->next;
    }
}
