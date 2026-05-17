//
//  LogEntryMetadataCs.hpp
//  KEXTLogKit
//
//  Created by Angus on 2026/5/16.
//

#ifndef LogEntryMetadataCs_hpp
#define LogEntryMetadataCs_hpp

#include <libkern/OSAtomic.h>
#include <IOKit/IOLib.h>  // 提供 IOMalloc / IOFree
#include <libkern/libkern.h>
#include <libkern/c++/OSObject.h>

/**
 * ------------------------------------------------------------------------
 * LogEntryMetadataCs
 * 描述：
 *   每条日志的元信息结构，用于环形缓冲区存储日志时附加信息
 * ------------------------------------------------------------------------
 */
struct LogEntryMetadataCs
{
    uint32_t level;           // 日志等级，例如 0=INFO, 1=WARNING, 2=ERROR
    uint32_t length;          // 日志内容长度（不含元信息）
    uint64_t timestamp;       // 时间戳，可用 mach_absolute_time 或其他时间
    char senderIdent[64];     // 发送该日志的 kext identification，固定长度
};

/**
 * ------------------------------------------------------------------------
 * 日志等级枚举
 * ------------------------------------------------------------------------
 */
enum LogLevel
{
    LOG_LEVEL_INFO = 0,
    LOG_LEVEL_WARNING = 1,
    LOG_LEVEL_ERROR = 2,
    LOG_LEVEL_DEBUG = 3
};

#endif /* LogEntryMetadataCs_hpp */
