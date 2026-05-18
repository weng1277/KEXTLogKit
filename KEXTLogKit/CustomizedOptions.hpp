//
//  CustomizedOptions.hpp
//  KEXTLogKit
//
//  Created by Angus on 2026/5/18.
//

#ifndef CustomizedOptions_hpp
#define CustomizedOptions_hpp



/**
 * @brief 驱动动态配置状态结构体（采用底层位域设计）
 * 每一项都死死限制在 1 个比特，非 0 即 1，极其节省内核内存且易于扩展。
 */
struct DriverStatus {
    uint8_t KEXTEnabled : 1;  // 0 或 1 (占 1 bit)
    uint8_t SelfOUTPUT  : 1;  // 0 或 1 (占 1 bit)
    
    // 以后如果你想扩展新功能，直接在这里秒加：
    // uint8_t useAggressiveFilter : 1; // 0 或 1
    // uint8_t dropCryptoLogs      : 1; // 0 或 1
};

class CustomizedOptions {
public:
    
    
    static DriverStatus parseInitDictionary(OSDictionary* dict);
    static DriverStatus parseProperties(IOService* provider);
};
#endif /* CustomizedOptions_hpp */
