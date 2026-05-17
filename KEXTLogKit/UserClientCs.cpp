//
//   UserClientCs.cpp
//   KEXTLogKit
//
//   Created by Angus on 2026/5/16.
//

#include "KEXTLogKit.hpp"
#include "UserClientCs.hpp"
#include <IOKit/IOLib.h>

#define super IOUserClient
OSDefineMetaClassAndStructors(UserClientCs, IOUserClient)

// -----------------------------------------------------------------------------
// 前置静态路由函数声明
// -----------------------------------------------------------------------------
static IOReturn sReadAllLogs(UserClientCs* target, void* reference, IOExternalMethodArguments* args);
static IOReturn sReadStreamLogs(UserClientCs* target, void* reference, IOExternalMethodArguments* args);
static IOReturn sClearLogs(UserClientCs* target, void* reference, IOExternalMethodArguments* args);
static IOReturn sReadRawBuffer(UserClientCs* target, void* reference, IOExternalMethodArguments* args);

// -----------------------------------------------------------------------------
// 【关键修复】IOKit 外部方法显式调度表
// -----------------------------------------------------------------------------
const IOExternalMethodDispatch UserClientCs::sMethods[kKEXTLogKitMethodCount] = {
    [0] = {
        .function                 = (IOExternalMethodAction)&sReadAllLogs,
        .checkScalarInputCount    = 0,
        .checkStructureInputSize  = 0,
        .checkScalarOutputCount   = 0,
        .checkStructureOutputSize = kIOUCVariableStructureSize // 放行 readall 描述符大通道
    },
    [1] = {
        .function                 = (IOExternalMethodAction)&sReadStreamLogs,
        // ⚡️【核心修复】绝对不能填变长标帜！这里明确指定只接收 1 个标量参数（isFirstCall），ioreg 即可通过安全校验
        .checkScalarInputCount    = 1,
        .checkStructureInputSize  = 0,
        .checkScalarOutputCount   = 0,
        .checkStructureOutputSize = kIOUCVariableStructureSize // 允许流式数据变长返回
    },
    [2] = {
        .function                 = (IOExternalMethodAction)&sClearLogs,
        .checkScalarInputCount    = 0,
        .checkStructureInputSize  = 0,
        .checkScalarOutputCount   = 0,
        .checkStructureOutputSize = 0
    },
    [3] = {
        .function                 = (IOExternalMethodAction)&sReadRawBuffer,
        .checkScalarInputCount    = 0,
        .checkStructureInputSize  = 0,
        .checkScalarOutputCount   = 0,
        .checkStructureOutputSize = kIOUCVariableStructureSize
    }
};

// -----------------------------------------------------------------------------
// 启动 UserClient
// -----------------------------------------------------------------------------
bool UserClientCs::start(IOService* provider)
{
    if (!super::start(provider))
        return false;

    targetService = OSDynamicCast(KEXTLogKit, provider);
    if (!targetService)
        return false;

    clientTask = current_task();
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
    if (selector < kKEXTLogKitMethodCount) {
        dispatch = (IOExternalMethodDispatch*)&sMethods[selector];
        if (!target) {
            target = this;
        }
        return super::externalMethod(selector, args, dispatch, target, reference);
    }
        
    return kIOReturnUnsupported;
}

// -----------------------------------------------------------------------------
// 【Selector 0】读取全部日志
// -----------------------------------------------------------------------------
static IOReturn sReadAllLogs(UserClientCs* target, void* reference, IOExternalMethodArguments* args)
{
    if (!target || !args) return kIOReturnBadArgument;
    
    KEXTLogKit* service = target->getTargetService();
    if (!service) return kIOReturnNotAttached;

    IOMemoryDescriptor* descriptor = args->structureOutputDescriptor;
    if (!descriptor) return kIOReturnBadArgument;

    size_t maxLen = descriptor->getLength();
    if (maxLen == 0) return kIOReturnBadArgument;

    IOReturn kr = descriptor->prepare(kIOMemoryDirectionOut);
    if (kr != kIOReturnSuccess) return kr;

    IOMemoryMap* memoryMap = descriptor->map();
    if (!memoryMap) {
        descriptor->complete(kIOMemoryDirectionOut);
        return kIOReturnVMError;
    }

    char* outBuffer = (char*)memoryMap->getVirtualAddress();
    if (!outBuffer) {
        memoryMap->release();
        descriptor->complete(kIOMemoryDirectionOut);
        return kIOReturnVMError;
    }

    size_t copied = service->readAllLogs(outBuffer, maxLen);

    memoryMap->release();
    descriptor->complete(kIOMemoryDirectionOut);

    args->structureOutputDescriptorSize = (uint32_t)copied;
    args->structureOutputSize = 0;

    return kIOReturnSuccess;
}

// -----------------------------------------------------------------------------
// 【Selector 1】流式读取日志
// -----------------------------------------------------------------------------
static IOReturn sReadStreamLogs(UserClientCs* target, void* reference, IOExternalMethodArguments* args)
{
    if (!target || !args) return kIOReturnBadArgument;
    
    KEXTLogKit* service = target->getTargetService();
    if (!service) return kIOReturnNotAttached;

    CircularLogCs* logBuf = service->getLogBuffer();
    if (!logBuf) return kIOReturnInternalError;

    char* outBuffer = (char*)args->structureOutput;
    size_t maxLen = args->structureOutputSize;
    
    if (!outBuffer || maxLen == 0) return kIOReturnBadArgument;

    // 【首呼叫处理】检测标量参数，如果为 1，清空积压历史
    if (args->scalarInputCount > 0 && args->scalarInput[0] == 1) {
        char dummyBuf[64];
        while (logBuf->getLength() > 0) {
            logBuf->readStream(dummyBuf, sizeof(dummyBuf));
        }
    }

    // 同步挂起休眠区
    while (logBuf->getLength() == 0) {
        IOSleep(40);
        if (target->isInactive()) {
            return kIOReturnAborted;
        }
    }

    if (maxLen > 1024) {
        maxLen = 1024;
    }

    size_t copied = logBuf->readStream(outBuffer, maxLen);
    args->structureOutputSize = (uint32_t)copied;
    
    return kIOReturnSuccess;
}

// -----------------------------------------------------------------------------
// 【Selector 2】清空日志
// -----------------------------------------------------------------------------
static IOReturn sClearLogs(UserClientCs* target, void* reference, IOExternalMethodArguments* args)
{
    if (!target) return kIOReturnBadArgument;
    
    KEXTLogKit* service = target->getTargetService();
    if (!service) return kIOReturnNotAttached;

    service->clearLogs();
    return kIOReturnSuccess;
}

// -----------------------------------------------------------------------------
// 【Selector 3】盲读物理裸数组
// -----------------------------------------------------------------------------
static IOReturn sReadRawBuffer(UserClientCs* target, void* reference, IOExternalMethodArguments* args)
{
    if (!target || !args) return kIOReturnBadArgument;
    
    KEXTLogKit* service = target->getTargetService();
    if (!service) return kIOReturnNotAttached;

    CircularLogCs* logBuf = service->getLogBuffer();
    if (!logBuf) return kIOReturnInternalError;

    IOMemoryDescriptor* descriptor = args->structureOutputDescriptor;
    if (!descriptor) return kIOReturnBadArgument;

    size_t maxLen = descriptor->getLength();
    if (maxLen == 0) return kIOReturnBadArgument;

    IOReturn kr = descriptor->prepare(kIOMemoryDirectionOut);
    if (kr != kIOReturnSuccess) return kr;

    IOMemoryMap* memoryMap = descriptor->map();
    if (!memoryMap) {
        descriptor->complete(kIOMemoryDirectionOut);
        return kIOReturnVMError;
    }

    char* outBuffer = (char*)memoryMap->getVirtualAddress();
    if (!outBuffer) {
        memoryMap->release();
        descriptor->complete(kIOMemoryDirectionOut);
        return kIOReturnVMError;
    }

    size_t copied = logBuf->readRawBuffer(outBuffer, maxLen, 0);

    memoryMap->release();
    descriptor->complete(kIOMemoryDirectionOut);

    args->structureOutputDescriptorSize = (uint32_t)copied;
    args->structureOutputSize = 0;
    
    return kIOReturnSuccess;
}

// -----------------------------------------------------------------------------
// 辅助成员函数
// -----------------------------------------------------------------------------
KEXTLogKit* UserClientCs::getTargetService() const { return targetService; }
IOReturn UserClientCs::clearLogs() { if (!targetService) return kIOReturnNotAttached; targetService->clearLogs(); return kIOReturnSuccess; }
IOReturn UserClientCs::readAllLogs(char* outBuffer, size_t maxLen) { if (!targetService || !outBuffer || maxLen == 0) return kIOReturnBadArgument; size_t copied = targetService->readAllLogs(outBuffer, maxLen); return (copied > 0) ? kIOReturnSuccess : kIOReturnNoSpace; }
IOReturn UserClientCs::readStreamLogs(char* outBuffer, size_t maxLen) { if (!targetService || !outBuffer || maxLen == 0) return kIOReturnBadArgument; size_t copied = targetService->readStreamLogs(outBuffer, maxLen); return (copied > 0) ? kIOReturnSuccess : kIOReturnNoSpace; }
