# 1.1watsed：too small exchanged buffer

```cpp

/*
#ifndef UserClientCs_hpp
#define UserClientCs_hpp

class KEXTLogKit;
class UserClientCs : public IOUserClient
{
    OSDeclareDefaultStructors(UserClientCs)

private:
    KEXTLogKit* targetService;   // 指向主 KEXTLogKit 服务对象
    task_t clientTask;

public:
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual IOReturn clientClose() override;
    virtual IOReturn clientDied() override;

    virtual IOReturn externalMethod(
        uint32_t selector,
        IOExternalMethodArguments* args,
        IOExternalMethodDispatch* dispatch,
        OSObject* target,
        void* reference) override;

    IOReturn readAllLogs(char* outBuffer, size_t maxLen);
    IOReturn readStreamLogs(char* outBuffer, size_t maxLen);
    IOReturn clearLogs();
};

#endif 
*/





/*
//
//  UserClientCs.cpp
//  KEXTLogKit
//
//  Created by Angus on 2026/5/16.
//
#include "KEXTLogKit.hpp"
#include "UserClientCs.hpp"
#include <IOKit/IOLib.h>

#define super IOUserClient
OSDefineMetaClassAndStructors(UserClientCs, IOUserClient)


#define KEXTLOGKIT_USERCLIENT_SELECTOR_READALL_RAW 3
// -----------------------------------------------------------------------------
// 启动 UserClient
// -----------------------------------------------------------------------------
bool UserClientCs::start(IOService* provider)
{
    if (!super::start(provider))
        return false;

    // 指向合并后的主服务 KEXTLogKit
    targetService = OSDynamicCast(KEXTLogKit, provider);
    if (!targetService)
        return false;

    clientTask = current_task(); // 保存用户任务
    IOLog("UserClientCs: started for task %p\n", clientTask);
    return true;
}

// -----------------------------------------------------------------------------
// 停止 UserClient
// -----------------------------------------------------------------------------
void UserClientCs::stop(IOService* provider)
{
    IOLog("UserClientCs: stopping\n");
    super::stop(provider);
}

// -----------------------------------------------------------------------------
// 用户端关闭连接
// -----------------------------------------------------------------------------
IOReturn UserClientCs::clientClose()
{
    IOLog("UserClientCs: clientClose\n");
    terminate();
    return kIOReturnSuccess;
}

// -----------------------------------------------------------------------------
// 用户端异常退出
// -----------------------------------------------------------------------------
IOReturn UserClientCs::clientDied()
{
    IOLog("UserClientCs: clientDied\n");
    return clientClose();
}

// -----------------------------------------------------------------------------
// 外部方法统一入口
// -----------------------------------------------------------------------------
IOReturn UserClientCs::externalMethod(
    uint32_t selector,
    IOExternalMethodArguments* args,
    IOExternalMethodDispatch* dispatch,
    OSObject* target,
    void* reference)
{
    if (!targetService)
        return kIOReturnNotAttached;

    if (!args)
        return kIOReturnBadArgument;

    switch (selector)
    {
        case 0: // 读取全部日志
        {
            char* outBuffer = (char*)args->structureOutput;
            size_t maxLen = args->structureOutputSize;
            
            if (!outBuffer || maxLen == 0)
                return kIOReturnBadArgument;

            // 调用主服务读取
            size_t copied = targetService->readAllLogs(outBuffer, maxLen);
            
            // 【核心修复】必须回写实际读取的长度给 IOKit 框架
            args->structureOutputSize = (uint32_t)copied;
            
            return kIOReturnSuccess; // 即使读到 0 字节也是成功，长度为 0 而已
        }

        case 1: // 流式读取日志
        {
            char* outBuffer = (char*)args->structureOutput;
            size_t maxLen = args->structureOutputSize;
            
            if (!outBuffer || maxLen == 0)
                return kIOReturnBadArgument;

            size_t copied = targetService->readStreamLogs(outBuffer, maxLen);
            
            // 【核心修复】回写流式读取长度
            args->structureOutputSize = (uint32_t)copied;
            
            return kIOReturnSuccess;
        }

        case 2: // 清空日志
            targetService->clearLogs();
            return kIOReturnSuccess;
        case KEXTLOGKIT_USERCLIENT_SELECTOR_READALL_RAW:
                {
                    char* outBuffer = (char*)args->structureOutput;
                    size_t maxLen = args->structureOutputSize;

                    if (!outBuffer || maxLen == 0) return kIOReturnBadArgument;

                    CircularLogCs* logBuf = targetService->getLogBuffer();
                    if (!logBuf) return kIOReturnInternalError;

                    // 【关键防御】如果应用层传来的缓冲大于 4096 字节，强行卡在 4096，防止被 IOKit 框架拒绝
                    if (maxLen > 4096) {
                        maxLen = 4096;
                    }

                    size_t copied = logBuf->readRawBuffer(outBuffer, maxLen, 0);

                    // 回写实际长度
                    args->structureOutputSize = (uint32_t)copied;
                    return kIOReturnSuccess;
                }

        default:
            return kIOReturnUnsupported;
    }
}
// -----------------------------------------------------------------------------
// 读取全部日志
// -----------------------------------------------------------------------------
IOReturn UserClientCs::readAllLogs(char* outBuffer, size_t maxLen)
{
    if (!targetService || !outBuffer || maxLen == 0)
        return kIOReturnBadArgument;

    size_t copied = targetService->readAllLogs(outBuffer, maxLen);
    return (copied > 0) ? kIOReturnSuccess : kIOReturnNoSpace;
}

// -----------------------------------------------------------------------------
// 流式读取日志
// -----------------------------------------------------------------------------
IOReturn UserClientCs::readStreamLogs(char* outBuffer, size_t maxLen)
{
    if (!targetService || !outBuffer || maxLen == 0)
        return kIOReturnBadArgument;

    size_t copied = targetService->readStreamLogs(outBuffer, maxLen);
    return (copied > 0) ? kIOReturnSuccess : kIOReturnNoSpace;
}

// -----------------------------------------------------------------------------
// 清空日志
// -----------------------------------------------------------------------------
IOReturn UserClientCs::clearLogs()
{
    if (!targetService)
        return kIOReturnNotAttached;

    targetService->clearLogs();
    return kIOReturnSuccess;
}
*/

