//
//  TempBuffercs.cpp
//  KEXTLogKit
//
//  Created by Angus on 2026/5/16.
//

#include "TempBufferCs.hpp"
#include <libkern/OSMalloc.h> // 内核 malloc/free
#include <string.h>           // memcpy, memset

// -----------------------------------------------------------------------------
// 私有构造函数
// -----------------------------------------------------------------------------
TempBuffer::TempBuffer(size_t cap)
    : data(nullptr), length(0), capacity(cap)
{
    if (cap > 0)
    {
        data = (char*)IOMalloc(cap);
    }
}

// -----------------------------------------------------------------------------
// 析构函数
// -----------------------------------------------------------------------------
TempBuffer::~TempBuffer()
{
    secureClear();
    if (data)
    {
        IOFree(data, capacity);
        data = nullptr;
    }
    length = 0;
    capacity = 0;
}

// -----------------------------------------------------------------------------
// 移动构造
// -----------------------------------------------------------------------------
TempBuffer::TempBuffer(TempBuffer&& other) noexcept
    : data(other.data), length(other.length), capacity(other.capacity)
{
    other.data = nullptr;
    other.length = 0;
    other.capacity = 0;
}

// -----------------------------------------------------------------------------
// 移动赋值
// -----------------------------------------------------------------------------
TempBuffer& TempBuffer::operator=(TempBuffer&& other) noexcept
{
    if (this != &other)
    {
        secureClear();
        if (data)
        {
            IOFree(data, capacity);
        }

        data = other.data;
        length = other.length;
        capacity = other.capacity;

        other.data = nullptr;
        other.length = 0;
        other.capacity = 0;
    }
    return *this;
}

// -----------------------------------------------------------------------------
// 工厂函数：仅分配容量，不带数据
// -----------------------------------------------------------------------------
TempBuffer* TempBuffer::withCapacity(size_t cap)
{
    TempBuffer* buf = new TempBuffer(cap);
    // length 默认 0，保证每次拿到都是空的
    return buf;
}

// -----------------------------------------------------------------------------
// 工厂函数：分配容量并拷贝数据
// 但我们可以立即 clear() 保证 buffer 是空的
// -----------------------------------------------------------------------------
TempBuffer* TempBuffer::withData(const char* src, size_t len)
{
    TempBuffer* buf = new TempBuffer(len);
    if (buf && src && len > 0)
    {
        memcpy(buf->data, src, len);
        buf->length = len;
        // 如果要求返回空 buffer，则立即清空
        buf->clear();
    }
    return buf;
}

// -----------------------------------------------------------------------------
// 获取数据指针
// -----------------------------------------------------------------------------
char* TempBuffer::getData() const
{
    return data;
}

// -----------------------------------------------------------------------------
// 获取有效长度
// -----------------------------------------------------------------------------
size_t TempBuffer::getLength() const
{
    return length;
}

// -----------------------------------------------------------------------------
// 获取容量
// -----------------------------------------------------------------------------
size_t TempBuffer::getCapacity() const
{
    return capacity;
}

// -----------------------------------------------------------------------------
// 清空数据，但不释放内存
// -----------------------------------------------------------------------------
void TempBuffer::clear()
{
    length = 0;
}

// -----------------------------------------------------------------------------
// 安全追加数据，自动扩容
// -----------------------------------------------------------------------------
bool TempBuffer::append(const char* src, size_t len)
{
    if (!src || len == 0) return true;

    // 自动扩容策略：2倍扩容，保证性能
    if (length + len > capacity)
    {
        size_t newCap = (capacity == 0) ? len : capacity;
        while (newCap < length + len) newCap *= 2;

        char* newData = (char*)IOMalloc(newCap);
        if (!newData) return false;

        if (data && length > 0)
            memcpy(newData, data, length);

        if (data) IOFree(data, capacity);
        data = newData;
        capacity = newCap;
    }

    memcpy(data + length, src, len);
    length += len;
    return true;
}

// -----------------------------------------------------------------------------
// 拷贝数据到目标内存
// -----------------------------------------------------------------------------
size_t TempBuffer::copyTo(char* dest, size_t len) const
{
    if (!dest || length == 0) return 0;

    size_t toCopy = (len < length) ? len : length;
    memcpy(dest, data, toCopy);
    return toCopy;
}

// -----------------------------------------------------------------------------
// 安全清零内部数据
// -----------------------------------------------------------------------------
void TempBuffer::secureClear()
{
    if (data && length > 0)
    {
        memset(data, 0, length);
    }
    length = 0;
}
