//
//  CustomizedOptions.cpp
//  KEXTLogKit
//
//  Created by Angus on 2026/5/18.
//
#include <libkern/libkern.h>
#include <IOKit/IOService.h>
#include "CustomizedOptions.hpp"
#include <libkern/c++/OSBoolean.h>
#include <IOKit/IOLib.h>
DriverStatus CustomizedOptions::parseProperties(IOService* provider)
{
    DriverStatus status;
    
    // 1. 默认兜底状态（0 或 1）
    status.KEXTEnabled = 1; // 默认开启
    status.SelfOUTPUT  = 0; // 默认关闭

    // 如果实例非法，直接返回默认值
    if (!provider) {
        IOLog("KEXTLogKit::CustomizedOptions: Invalid provider. Using defaults.\n");
        return status;
    }

    // 2. 解析 "KEXTEnabled"
    OSObject* objEnabled = provider->getProperty("KEXTEnabled");
    if (objEnabled) {
        OSBoolean* boolEnabled = OSDynamicCast(OSBoolean, objEnabled);
        if (boolEnabled) {
            status.KEXTEnabled = boolEnabled->isTrue() ? 1 : 0;
        }
    }

    // 3. 解析 "SelfOUTPUT"
    OSObject* objOutput = provider->getProperty("SelfOUTPUT");
    if (objOutput) {
        OSBoolean* boolOutput = OSDynamicCast(OSBoolean, objOutput);
        if (boolOutput) {
            status.SelfOUTPUT = boolOutput->isTrue() ? 1 : 0;
        }
    }

    // 以后扩展时，只需在这里复制粘贴，解析新的字段即可：
    // OSObject* objNew = provider->getProperty("YourNewKey");
    // if (objNew) { ... status.useAggressiveFilter = ... }

    IOLog("KEXTLogKit::CustomizedOptions: Parsing complete. [KEXTEnabled: %d] [SelfOUTPUT: %d]\n",
          status.KEXTEnabled, status.SelfOUTPUT);

    return status;
}
DriverStatus CustomizedOptions::parseInitDictionary(OSDictionary* dict)
{
    DriverStatus status;
    // 1. 设置默认安全值
    status.KEXTEnabled = 1; // 默认开启
    status.SelfOUTPUT  = 1; // 默认关闭

    if (!dict) {
        IOLog("KEXTLogKit::CustomizedOptions: Received null dictionary in init.\n");
        return status;
    }

    // 2. 剥洋葱：IOKitPersonalities -> kextlogkit
    OSDictionary* personalities = OSDynamicCast(OSDictionary, dict->getObject("IOKitPersonalities"));
    if (!personalities) return status;

    // 💡 注意：这里的 "kextlogkit" 必须和你在 Info.plist 里的 IOKitPersonalities 下填的键名完全一致
    OSDictionary* myPersonality = OSDynamicCast(OSDictionary, personalities->getObject("kextlogkit"));
    if (!myPersonality) return status;

    // 3. --- 统一解析具体开关（高度可扩展区域） ---
    
    // 解析 KEXTEnabled
    OSObject* objEnabled = myPersonality->getObject("KEXTEnabled");
    if (objEnabled) {
        OSBoolean* boolEnabled = OSDynamicCast(OSBoolean, objEnabled);
        if (boolEnabled) {
            status.KEXTEnabled = boolEnabled->isTrue() ? 1 : 0;
        }
    }

    // 解析 SelfOUTPUT
    OSObject* objOutput = myPersonality->getObject("SelfOUTPUT");
    if (objOutput) {
        OSBoolean* boolOutput = OSDynamicCast(OSBoolean, objOutput);
        if (boolOutput) {
            status.SelfOUTPUT = boolOutput->isTrue() ? 1 : 0;
        }
    }

    // 💡 以后如果需要扩展第三个开关，直接在这里续写：
    // OSObject* objNew = myPersonality->getObject("YourNewKey");
    // if (objNew) { ... status.newKey = ... }

    IOLog("KEXTLogKit::CustomizedOptions: [Init Dict Parsed] KEXTEnabled: %d, SelfOUTPUT: %d\n",
          status.KEXTEnabled, status.SelfOUTPUT);

    return status;
}
