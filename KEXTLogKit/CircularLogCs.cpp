//
//   CircularLogCs.cpp
//   KEXTLogKit
//

#include "CircularLogCs.hpp"
#include "KEXTLogKit.hpp"
#include <IOKit/IOLib.h>

CircularLogCs::CircularLogCs(size_t cap)
    : buffer(nullptr), capacity(0), head(0), tail(0), full(false)
{
    if (cap > 0) {
        buffer = (char*)IOMalloc(cap);
        if (buffer) {
            bzero(buffer, cap);
            capacity = cap;
        }
    }
}

CircularLogCs::~CircularLogCs()
{
    if (buffer) {
        IOFree(buffer, capacity);
        buffer = nullptr;
    }
    capacity = 0;
    head = 0;
    tail = 0;
    full = false;
}

CircularLogCs* CircularLogCs::create(size_t cap)
{
    if (cap == 0) return nullptr;
    CircularLogCs* logObj = new CircularLogCs(cap);
    if (!logObj) return nullptr;
    if (!logObj->buffer) {
        delete logObj;
        return nullptr;
    }
    return logObj;
}

bool CircularLogCs::write(const char* buf, size_t len)
{
    if (!buf || len == 0 || capacity == 0) return false;
    TaskLock::Guard guard(lock);

    if (len >= capacity) {
        buf += (len - capacity);
        len = capacity;
        memcpy(buffer, buf, capacity);
        head = 0;
        tail = 0;
        full = true;
        return true;
    }

    size_t currentLen = 0;
    if (full) currentLen = capacity;
    else if (head >= tail) currentLen = head - tail;
    else currentLen = capacity + head - tail;

    if (currentLen + len > capacity) {
        size_t overwriteLen = (currentLen + len) - capacity;
        tail = (tail + overwriteLen) % capacity;
        full = true;
    }

    size_t firstChunk = (len < capacity - head) ? len : (capacity - head);
    memcpy(buffer + head, buf, firstChunk);
    
    size_t remaining = len - firstChunk;
    if (remaining > 0) {
        memcpy(buffer, buf + firstChunk, remaining);
    }
    
    head = (head + len) % capacity;
    if (head == tail) full = true;

    return true;
}

size_t CircularLogCs::readRawBuffer(char* outBuffer, size_t maxLen, size_t offset)
{
    if (!outBuffer || maxLen == 0 || capacity == 0) return 0;
    TaskLock::Guard guard(lock);
    if (offset >= capacity) return 0;

    size_t available = capacity - offset;
    size_t bytesToCopy = (available < maxLen) ? available : maxLen;
    if (bytesToCopy > 0) memcpy(outBuffer, buffer + offset, bytesToCopy);
    return bytesToCopy;
}

size_t CircularLogCs::readAll(char* outBuffer, size_t maxLen)
{
    if (!outBuffer || maxLen == 0 || capacity == 0) return 0;
    TaskLock::Guard guard(lock);

    size_t currentLen = 0;
    if (full) currentLen = capacity;
    else if (head >= tail) currentLen = head - tail;
    else currentLen = capacity + head - tail;

    size_t copyLen = (currentLen < maxLen) ? currentLen : maxLen;
    if (copyLen == 0) return 0;

    size_t firstChunk = (copyLen < capacity - tail) ? copyLen : (capacity - tail);
    memcpy(outBuffer, buffer + tail, firstChunk);

    size_t remaining = copyLen - firstChunk;
    if (remaining > 0) {
        memcpy(outBuffer + firstChunk, buffer, remaining);
    }

    return copyLen;
}

// -----------------------------------------------------------------------------
// 流式数据消费实现
// -----------------------------------------------------------------------------
size_t CircularLogCs::readStream(char* outBuffer, size_t maxLen)
{
    if (!outBuffer || maxLen == 0 || capacity == 0) return 0;
    TaskLock::Guard guard(lock);

    size_t currentLen = 0;
    if (full) currentLen = capacity;
    else if (head >= tail) currentLen = head - tail;
    else currentLen = capacity + head - tail;

    size_t copyLen = (currentLen < maxLen) ? currentLen : maxLen;
    if (copyLen == 0) return 0;

    size_t firstChunk = (copyLen < capacity - tail) ? copyLen : (capacity - tail);
    memcpy(outBuffer, buffer + tail, firstChunk);

    size_t remaining = copyLen - firstChunk;
    if (remaining > 0) {
        memcpy(outBuffer + firstChunk, buffer, remaining);
    }

    tail = (tail + copyLen) % capacity;
    full = false; // 只要消费了数据，就不可能是满状态

    return copyLen;
}

void CircularLogCs::clear()
{
    TaskLock::Guard guard(lock);
    head = 0; tail = 0; full = false;
    if (buffer) bzero(buffer, capacity);
}

size_t CircularLogCs::getLength() const
{
    TaskLock::Guard guard(lock);
    if (full) return capacity;
    if (head >= tail) return head - tail;
    return capacity + head - tail;
}

size_t CircularLogCs::getCapacity() const
{
    return capacity;
}

size_t CircularLogCs::getAvailableStreamSize() const
{
    return getLength();
}
