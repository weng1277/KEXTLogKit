//
//  CircularLogCs.hpp
//  KEXTLogKit
//
//  Created by Angus on 2026/5/16.
//

#ifndef CircularLogCs_hpp
#define CircularLogCs_hpp

#include <libkern/OSMalloc.h>
#include <string.h>
#include "TaskLock.hpp"
#include "TempBufferCs.hpp"

/**
 * ------------------------------------------------------------------------
 * CircularLogCs
 *
 * 内核态环形日志缓冲区
 * ---------------------
 * 描述：
 *   提供一个固定容量的环形缓冲区，用于内核日志存储。
 *   支持：
 *     - 多线程/多任务安全（内部使用 TaskLock 保护）
 *     - 一次性读取全部日志
 *     - 流式读取日志（分块读取，tail 指针前进）
 *     - 自动覆盖最旧日志（写满后）
 *
 * 使用示例：
 *   // 创建 1MB 环形日志
 *   CircularLogCs* log = CircularLogCs::create(1024 * 1024);
 *
 *   // 写入日志
 *   log->write("Hello kernel log", 16);
 *
 *   // 读取全部
 *   char buf[512];
 *   size_t len = log->readAll(buf, sizeof(buf));
 *
 *   // 清空日志
 *   log->clear();
 * ------------------------------------------------------------------------
 */
class CircularLogCs
{
private:
    char* buffer;           // 环形缓冲区指针
    size_t capacity;        // 缓冲区总大小（字节数）
    size_t head;            // 写入指针（下一个写入位置）
    size_t tail;            // 读取指针（流式读取用）
    bool full;              // 是否已写满
   
    mutable TaskLock lock;        // 内核锁，保护 buffer/head/tail/full

    // 构造函数私有化，通过工厂函数创建
    explicit CircularLogCs(size_t cap);

public:
    // 禁止拷贝
    // 盲读接口：无视指针状态，直接将底层 raw buffer 的特定区间拷贝出来
    size_t readRawBuffer(char* outBuffer, size_t maxLen, size_t offset);
    CircularLogCs(const CircularLogCs&) = delete;
    CircularLogCs& operator=(const CircularLogCs&) = delete;

    CircularLogCs(CircularLogCs&& other) = delete;
    CircularLogCs& operator=(CircularLogCs&& other) = delete;

    // 析构函数，释放缓冲区
    ~CircularLogCs();

    // ---------------------------------------------------------------------
    // 静态工厂函数
    // 创建环形日志对象
    // @param cap: 缓冲区容量（字节）
    // @return 成功返回 CircularLogCs*，失败返回 nullptr
    // ---------------------------------------------------------------------
    static CircularLogCs* create(size_t cap);

    // ---------------------------------------------------------------------
    // 写入日志数据
    // @param buf: 待写入数据指针
    // @param len: 数据长度
    // @return true 写入成功 / false 失败
    // ---------------------------------------------------------------------
    bool write(const char* buf, size_t len);

    // ---------------------------------------------------------------------
    // 一次性读取全部日志
    // @param outBuffer: 用户缓冲区指针
    // @param maxLen: 用户缓冲区容量
    // @return 实际拷贝字节数
    // ---------------------------------------------------------------------
    size_t readAll(char* outBuffer, size_t maxLen);

    // ---------------------------------------------------------------------
    // 流式读取日志
    // 每次调用从 tail 开始读取，tail 前进
    // @param outBuffer: 用户缓冲区指针
    // @param maxLen: 用户缓冲区容量
    // @return 实际读取字节数
    // ---------------------------------------------------------------------
    size_t readStream(char* outBuffer, size_t maxLen);

    // ---------------------------------------------------------------------
    // 清空日志缓冲区
    // ---------------------------------------------------------------------
    void clear();

    // ---------------------------------------------------------------------
    // 获取当前已写入的数据长度
    // ---------------------------------------------------------------------
    size_t getLength() const;

    // ---------------------------------------------------------------------
    // 获取缓冲区总容量
    // ---------------------------------------------------------------------
    size_t getCapacity() const;
    
    
    size_t getAvailableStreamSize() const;
};

#endif /* CircularLogCs_hpp */
