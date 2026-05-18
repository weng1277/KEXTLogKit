//
//  KEXTLogKit.hpp
//  KEXTLogKit
//
//  Created by Angus on 2026/5/16.
//

#ifndef KEXTLogKit_hpp
#define KEXTLogKit_hpp

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include "CircularLogCs.hpp"
#include "BufferWriteInListCs.hpp"
#include "TempBufferCs.hpp"
#include "TaskLock.hpp"
#include "UserClientCs.hpp"
#include "CustomizedOptions.hpp"
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
class KEXTLogKit : public IOService
{
    OSDeclareDefaultStructors(KEXTLogKit)

private:
    static DriverStatus s_status;
    CircularLogCs* logBuffer;
    BufferWriteInListCs* writeQueue;
    TaskLock internalLock;
    static KEXTLogKit* s_instance;
    IOWorkLoop*         workLoop;
    IOTimerEventSource* timerSource;
    bool writeInternalLog(uint32_t level, const char* data, size_t len);
    // 定时器回调函数
    void timerFired(IOTimerEventSource* sender);
    
public:
    static KEXTLogKit* getInstance();
    CircularLogCs* getLogBuffer() const { return logBuffer; }
    // IOService 生命周期
    virtual bool init(OSDictionary* dictionary = nullptr) override;
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;

    // 日志接口
    bool writeLogFromKext(const char* ident, uint32_t level, const char* data, size_t len);
    
    void flushQueueToLog();
    size_t readAllLogs(char* outBuffer, size_t maxLen);
    size_t readStreamLogs(char* outBuffer, size_t maxLen);
    void clearLogs();
    size_t getLogCapacity() const;
    size_t getLogLength() const;

    // UserClient 创建
    virtual IOReturn newUserClient(task_t owningTask,
                                   void* securityID,
                                   UInt32 type,
                                   OSDictionary* properties,
                                   IOUserClient** handler) override;
};

#endif /* KEXTLogKit_hpp */
