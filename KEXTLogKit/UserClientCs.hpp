#pragma once
#include <IOKit/IOUserClient.h>

class KEXTLogKit;

class UserClientCs : public IOUserClient
{
    OSDeclareDefaultStructors(UserClientCs)

protected:
    KEXTLogKit* targetService;
    task_t      clientTask;

    // 对应 4 个 selector 的分发枚举大小
    enum {
        kKEXTLogKitMethodCount = 4
    };

    // 静态调度分发描述表
    static const IOExternalMethodDispatch sMethods[kKEXTLogKitMethodCount];

public:
    virtual bool start(IOService* provider) override;
    virtual void stop(IOService* provider) override;
    virtual IOReturn clientClose() override;
    virtual IOReturn clientDied() override;
    
    virtual IOReturn externalMethod(uint32_t selector,
                                    IOExternalMethodArguments* args,
                                    IOExternalMethodDispatch* dispatch = nullptr,
                                    OSObject* target = nullptr,
                                    void* reference = nullptr) override;

    // 辅助获取接口
    KEXTLogKit* getTargetService() const;
    IOReturn readAllLogs(char* outBuffer, size_t maxLen);
    IOReturn readStreamLogs(char* outBuffer, size_t maxLen);
    IOReturn clearLogs();
};

