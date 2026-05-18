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
DriverStatus KEXTLogKit::s_status = {1, 1};

KEXTLogKit* KEXTLogKit::getInstance()
{
    return s_instance;
}

// -----------------------------------------------------------------------------
// 初始化 IOService
// -----------------------------------------------------------------------------
bool KEXTLogKit::init(OSDictionary* dictionary)
{
    // 1. 必须首先让基类初始化
    if (!super::init(dictionary))
        return false;
        
    // 2. 基类成功后，安全挂载全局单例
    s_instance = this;
    
    s_status = CustomizedOptions::parseInitDictionary(dictionary);
    size_t bufferSize = 1024 * 1024; // 1MB buffer
    
    if (s_status.KEXTEnabled == 0) {
        IOLog("KEXTLogKit: KEXTEnabled is 0 in init! Shrinking buffer to 1KB.\n");
        bufferSize = 1024;           // 成功降级为 1KB
    }
    
    // 3. 创建环形缓冲区
    logBuffer = CircularLogCs::create(bufferSize);
    if (!logBuffer) {
        s_instance = nullptr;        // 核心加固：失败退出前，必须清空单例指针，防止变成野指针
        return false;
    }

    // 4. 创建写入队列
    writeQueue = new BufferWriteInListCs();
    if (!writeQueue)
    {
        delete logBuffer;
        logBuffer = nullptr;
        s_instance = nullptr;        //  核心加固：同理，清理单例指针
        return false;
    }

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

    // 3. 【修改点】：根据开关状态决定是否启动定时器
        if (s_status.KEXTEnabled == 0) {
            // 最高优先级：安全模式强制开启 10 秒心跳，无视 SelfOUTPUT
            timerSource->setTimeoutMS(10000);
            IOLog("KEXTLogKit: started and registered with 10000ms timer (SAFE_MODE)\n");
        }
        else if (s_status.SelfOUTPUT != 0) {
            // 常规工作模式且允许输出自身日志：开启 5 秒心跳
            timerSource->setTimeoutMS(5000);
            IOLog("KEXTLogKit: started and registered with 5000ms timer\n");
        }
        else {
            // 常规工作模式但禁用了自身日志：【彻底关闭定时器】
            IOLog("KEXTLogKit: started and registered, but timer is DISABLED (SelfOUTPUT=0)\n");
            // 注意：这里故意不调用 setTimeoutMS，定时器就不会运行
        }
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
    if (s_status.KEXTEnabled != 0 && s_status.SelfOUTPUT == 0) {
            IOLog("KEXTLogKit::timerFired -> selfoutput is 0, cancelling timer to save CPU.\n");
            sender->cancelTimeout();
            return;
        }
    // 1. 内核打点：证明定时器确实进来了
    IOLog("KEXTLogKit::timerFired -> Triggered!\n");

    uint64_t uptime_abs = 0;
    uint64_t uptime_ns = 0;
    clock_get_uptime(&uptime_abs);
    absolutetime_to_nanoseconds(uptime_abs, &uptime_ns);
    uint64_t uptime_s = uptime_ns / 1000000000ULL;
    char msg[128];
    int msgLen = 0;
    uint32_t interval = 5000; // 默认 5000 毫秒（5秒）
    bool pushRet = false;
        // 【修改点 1】：根据开关状态，直接在这里切换输出内容和下一次的时间
        if (s_status.KEXTEnabled == 0) {
            interval = 10000; // 降级为 10000 毫秒（10秒）
            msgLen = snprintf(msg, sizeof(msg),
                              "[SAFE_MODE] System Uptime: %llu seconds. KEXT is currently DISABLED. \n",
                              uptime_s);
            pushRet = writeInternalLog(0, msg, msgLen);
        } else {
            interval = 5000;  // 维持 5000 毫秒（5秒）
            msgLen = snprintf(msg, sizeof(msg),
                              "System Uptime: %llu seconds. Background logging active.\n",
                              uptime_s);
            pushRet = writeInternalLog(1, msg, msgLen);
        }

    // 2. 写入队列
    
    
    // 3. 冲刷到环形缓冲区
    flushQueueToLog();

    // 4. 内核打点：观察当前环形缓冲区的实际数据量
    size_t currentLen = getLogLength();
    IOLog("KEXTLogKit::timerFired -> Pushed ret: %d, Current Buffer Total Len: %zu\n", pushRet, currentLen);

    // 5. 重新注册
    sender->setTimeoutMS(interval);
}
// ---------------- 日志接口 ----------------
bool KEXTLogKit::writeLogFromKext(const char* ident, uint32_t level, const char* data, size_t len)
{
    if (s_status.KEXTEnabled == 0) {
            return false;
        }
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

bool KEXTLogKit::writeInternalLog(uint32_t level, const char* data, size_t len)
{
    if (!data || len == 0) return false;

    TaskLock::Guard guard(internalLock);

    TempBuffer* temp = TempBuffer::withCapacity(len + 64);
    if (!temp) return false;

    char header[64];
    int headerLen = snprintf(header, sizeof(header), "[%s][%u] ",  "angus.KEXTLogKit", level);
    temp->append(header, headerLen);
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
