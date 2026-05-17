//
//  KEXTLogKit.cpp
//  KEXTLogKit
//
//  Created by Angus on 2026/5/16.
//

#include "KEXTLogKit.hpp"
#include "UserClientCs.hpp" 
#include <IOKit/IOLib.h>
#include <kern/clock.h>
#define super IOService
OSDefineMetaClassAndStructors(KEXTLogKit, IOService)

// 静态单例初始化
KEXTLogKit* KEXTLogKit::s_instance = nullptr;

KEXTLogKit* KEXTLogKit::getInstance()
{
    return s_instance;
}

// -----------------------------------------------------------------------------
// 初始化 IOService
// -----------------------------------------------------------------------------
bool KEXTLogKit::init(OSDictionary* dictionary)
{
    if (!super::init(dictionary))
        return false;

    logBuffer = CircularLogCs::create(1024 * 1024); // 1MB buffer
    if (!logBuffer)
        return false;

    writeQueue = new BufferWriteInListCs();
    if (!writeQueue)
    {
        delete logBuffer;
        logBuffer = nullptr;
        return false;
    }

    s_instance = this;
    IOLog("KEXTLogKit: init success\n");
    return true;
}

// -----------------------------------------------------------------------------
// 启动服务
// -----------------------------------------------------------------------------
bool KEXTLogKit::start(IOService* provider)
{
    if (!super::start(provider))
        return false;

    setName("KEXTLogKit");   // ioreg 显示的名称
    registerService(0);       // 用户空间可以打开

    // 1. 创建 WorkLoop
        workLoop = IOWorkLoop::workLoop();
        if (!workLoop) return false;

        // 2. 创建定时器事件源并绑定回调函数 timerFired
        timerSource = IOTimerEventSource::timerEventSource(this,
            OSMemberFunctionCast(IOTimerEventSource::Action, this, &KEXTLogKit::timerFired));
        
        if (!timerSource || workLoop->addEventSource(timerSource) != kIOReturnSuccess)
        {
            if (timerSource) timerSource->release();
            if (workLoop) workLoop->release();
            return false;
        }

        // 3. 启动第一次定时（5000毫秒）
        timerSource->setTimeoutMS(5000);

        IOLog("KEXTLogKit: started and registered with 5s timer\n");
    return true;
}

// -----------------------------------------------------------------------------
// 停止服务
// -----------------------------------------------------------------------------
void KEXTLogKit::stop(IOService* provider)
{
    IOLog("KEXTLogKit: stopping\n");

    // 1. 取消定时器并从 WorkLoop 中移除，防止内核崩溃
    if (timerSource)
    {
        timerSource->cancelTimeout();
        if (workLoop)
            workLoop->removeEventSource(timerSource);
        timerSource->release();
        timerSource = nullptr;
    }

    // 2. 释放 WorkLoop
    if (workLoop)
    {
        workLoop->release();
        workLoop = nullptr;
    }

    // 3. 清理残留日志和内存
    flushQueueToLog();
    clearLogs();

    if (writeQueue)
    {
        delete writeQueue;
        writeQueue = nullptr;
    }
    if (logBuffer)
    {
        delete logBuffer;
        logBuffer = nullptr;
    }

    s_instance = nullptr;
    super::stop(provider);
}
// -----------------------------------------------------------------------------
// 定时器回调函数：每 5 秒执行一次
// -----------------------------------------------------------------------------
void KEXTLogKit::timerFired(IOTimerEventSource* sender)
{
    if (!sender) return;

    // 1. 内核打点：证明定时器确实进来了
    IOLog("KEXTLogKit::timerFired -> Triggered!\n");

    uint64_t uptime_abs = 0;
    uint64_t uptime_ns = 0;
    clock_get_uptime(&uptime_abs);
    absolutetime_to_nanoseconds(uptime_abs, &uptime_ns);
    uint64_t uptime_s = uptime_ns / 1000000000ULL;

    char msg[128];
    int msgLen = snprintf(msg, sizeof(msg),
                          "System Uptime: %llu seconds. Background logging active.\n",
                          uptime_s);

    // 2. 写入队列
    bool pushRet = writeLogFromKext("angus.KEXTLogKit", 2, msg, msgLen);
    
    // 3. 冲刷到环形缓冲区
    flushQueueToLog();

    // 4. 内核打点：观察当前环形缓冲区的实际数据量
    size_t currentLen = getLogLength();
    IOLog("KEXTLogKit::timerFired -> Pushed ret: %d, Current Buffer Total Len: %zu\n", pushRet, currentLen);

    // 5. 重新注册
    sender->setTimeoutMS(5000);
}
// ---------------- 日志接口 ----------------
bool KEXTLogKit::writeLogFromKext(const char* ident, uint32_t level, const char* data, size_t len)
{
    if (!data || len == 0) return false;

    TaskLock::Guard guard(internalLock);

    TempBuffer* temp = TempBuffer::withCapacity(len + 64);
    if (!temp) return false;

    char header[64];
    int headerLen = snprintf(header, sizeof(header), "[%s][%u] ", ident ? ident : "unknown", level);
    temp->append(header, headerLen);
    temp->append(data, len);

    writeQueue->pushlist(temp);
    return true;
}

bool KEXTLogKit::writeInternalLog(const char* data, size_t len)
{
    if (!data || len == 0) return false;

    TaskLock::Guard guard(internalLock);

    TempBuffer* temp = TempBuffer::withCapacity(len);
    if (!temp) return false;

    temp->append(data, len);
    writeQueue->pushlist(temp);
    return true;
}

void KEXTLogKit::flushQueueToLog()
{
    TaskLock::Guard guard(internalLock);

    while (true)
    {
        TempBuffer* buf = writeQueue->dequeue();
        if (!buf) break;

        logBuffer->write(buf->getData(), buf->getLength());
        delete buf;
    }
}

size_t KEXTLogKit::readAllLogs(char* outBuffer, size_t maxLen)
{
    if (!outBuffer || maxLen == 0) return 0;

    // 先注释掉原本的环形缓冲读取
    TaskLock::Guard guard(internalLock);
    return logBuffer->readAll(outBuffer, maxLen);

    // 【硬编码测试】
    /*
    const char* testStr = "[KERNEL_TEST] Hello Angus, if you see this, IPC works!\n";
    size_t testLen = strlen(testStr);
    if (maxLen > testLen) {
        memcpy(outBuffer, testStr, testLen);
        return testLen;
    }
    return 0;*/
}

size_t KEXTLogKit::readStreamLogs(char* outBuffer, size_t maxLen)
{
    if (!outBuffer || maxLen == 0) return 0;

    TaskLock::Guard guard(internalLock);
    return logBuffer->readStream(outBuffer, maxLen);
}

void KEXTLogKit::clearLogs()
{
    TaskLock::Guard guard(internalLock);
    logBuffer->clear();
}

size_t KEXTLogKit::getLogCapacity() const
{
    return logBuffer ? logBuffer->getCapacity() : 0;
}

size_t KEXTLogKit::getLogLength() const
{
    return logBuffer ? logBuffer->getLength() : 0;
}

// ---------------- UserClient ----------------
IOReturn KEXTLogKit::newUserClient(task_t owningTask,
                                   void* securityID,
                                   UInt32 type,
                                   OSDictionary* properties,
                                   IOUserClient** handler)
{
    if (!handler)
        return kIOReturnBadArgument;

    // 1. 分配内存
    UserClientCs* client = OSTypeAlloc(UserClientCs);
    if (!client)
        return kIOReturnNoMemory;

    // 2. 必须先调用 IOKit 框架的初始化函数
    if (!client->initWithTask(owningTask, securityID, type, properties))
    {
        client->release();
        return kIOReturnBadArgument;
    }

    // 3. 将 UserClient 挂载到主服务
    if (!client->attach(this))
    {
        client->release();
        return kIOReturnUnsupported; // 或者返回相应的错误码
    }

    // 4. 启动 UserClient
    if (!client->start(static_cast<IOService*>(this)))
    {
        client->detach(this);
        client->release();
        return kIOReturnBadArgument;
    }

    // 5. 成功返回传出参数
    *handler = client;
    return kIOReturnSuccess;
}
