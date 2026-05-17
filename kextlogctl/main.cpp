//
//  main.cpp
//  KEXTLogKitCLI
//
//  Created by Angus on 2026/5/17.
//

#include <iostream>
#include <IOKit/IOKitLib.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <cstring>
#include <csignal>

#define KEXTLOGKIT_USERCLIENT_SELECTOR_READALL      0
#define KEXTLOGKIT_USERCLIENT_SELECTOR_READSTREAM   1
#define KEXTLOGKIT_USERCLIENT_SELECTOR_CLEAR        2
#define KEXTLOGKIT_USERCLIENT_SELECTOR_READALL_RAW  3

static volatile std::sig_atomic_t g_keep_stream_running = 1;

// 辅助函数：以 Hex + ASCII 格式优雅地倾倒裸内存（16字节对齐）
void hexDump(const void* addr, size_t len) {
    const unsigned char* pc = (const unsigned char*)addr;
    unsigned char buff[17];
    size_t i;

    for (i = 0; i < len; i++) {
        if ((i % 16) == 0) {
            if (i != 0) std::cout << "  " << buff << "\n";
            printf("  %04zx ", i);
        }
        printf(" %02x", pc[i]);
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {
            buff[i % 16] = '.';
        } else {
            buff[i % 16] = pc[i];
        }
        buff[(i % 16) + 1] = '\0';
    }
    while ((i % 16) != 0) {
        printf("    ");
        i++;
    }
    std::cout << "  " << buff << "\n";
}

int main() {
    mach_port_t masterPort;
    io_connect_t connect;
    kern_return_t kr;

    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr != KERN_SUCCESS) {
        std::cerr << "Failed to get master port: " << kr << "\n";
        return 1;
    }

    io_service_t service = IOServiceGetMatchingService(masterPort,
                                IOServiceMatching("KEXTLogKit"));
    if (!service) {
        std::cerr << "Failed to find KEXTLogKit service in IORegistry.\n";
        return 1;
    }

    kr = IOServiceOpen(service, mach_task_self(), 0, &connect);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        std::cerr << "Failed to open UserClient: " << kr << "\n";
        return 1;
    }

    std::cout << "Connected to KEXTLogKit.\n";

    bool running = true;
    while (running) {
        std::cout << "\nCommands: readall | readstream | clear | readraw | exit\n> ";
        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "readall") {
            const size_t bufSize = 1024 * 1024; // 1MB
            std::vector<char> buffer(bufSize, 0);
            size_t outSize = bufSize;

            kr = IOConnectCallMethod(
                connect,
                KEXTLOGKIT_USERCLIENT_SELECTOR_READALL,
                nullptr, 0,
                nullptr, 0,
                nullptr, nullptr,
                buffer.data(),
                &outSize
            );
            
            if (kr == KERN_SUCCESS) {
                if (outSize > 0) {
                    std::cout << "=== All Logs (" << outSize << " bytes) ===\n";
                    std::cout.write(buffer.data(), outSize);
                    std::cout << "\n========================\n";
                } else {
                    std::cout << "--- No log data available in buffer. ---\n";
                }
            } else {
                std::cerr << "Driver communication error (readall via Descriptor), kr = 0x" << std::hex << kr << std::dec << "\n";
            }
            
        } else if (cmd == "readstream") {
            std::cout << "\033[?25l"; // 隐藏终端光标
            std::cout << "\n--- Entering Live Stream Mode (Tail-F). Press Ctrl+C to exit. ---\n\n";

            // 1. 接管 SIGINT (Ctrl+C) 信号，允许平滑打断内核阻塞
            g_keep_stream_running = 1;
            struct sigaction old_action, action;
            action.sa_handler = [](int) { g_keep_stream_running = 0; };
            sigemptyset(&action.sa_mask);
            action.sa_flags = 0;
            sigaction(SIGINT, &action, &old_action);

            const size_t bufSize = 1024;
            char buffer[bufSize];

            // ⚡️ 快进控制标帜：第一次循环传 1 通知内核清空积压，随后变为 0
            uint64_t isFirstCall = 1;

            // 2. 核心流循环：依仗内核态 IOSleep 进行高效挂起，不占应用层 CPU
            while (g_keep_stream_running) {
                size_t outSize = bufSize;

                // ⚡️【核心修改】由于存在 Scalar Input + Structure Output 混合参数，
                // 必须使用通用的 `IOConnectCallMethod` 以匹配内核侧修改后的静态分发安全校验规则。
                kr = IOConnectCallMethod(
                    connect,
                    KEXTLOGKIT_USERCLIENT_SELECTOR_READSTREAM,
                    &isFirstCall, 1,            // 输入：1 个 Scalar 参数 (isFirstCall)
                    nullptr, 0,                 // 输入：无结构体
                    nullptr, nullptr,           // 输出：无 Scalar 参数返回
                    buffer, &outSize            // 输出：结构体缓冲区 (变长接收流数据)
                );
                
                if (kr == KERN_SUCCESS) {
                    // ⚡️ 只要内核首发冲刷顺利执行成功，立即将其降为 0，防止把后续新进来的实时流也过滤了
                    isFirstCall = 0;

                    if (outSize > 0) {
                        // 精准倾倒流式吐出的原生字节，不再包裹额外的打印框
                        std::cout.write(buffer, outSize);
                        std::flush(std::cout);
                    }
                } else if (kr == 0x2e2) { // kIOReturnAborted
                    // 捕获用户在挂起时强行中断退出信号
                    break;
                } else {
                    std::cerr << "\n[Stream Error] Connection exception, kr = 0x" << std::hex << kr << std::dec << "\n";
                    break;
                }
            }

            // 3. 释放信号接管并回复光标
            sigaction(SIGINT, &old_action, nullptr);
            std::cout << "\033[?25h"; // 恢复光标
            std::cout << "\n--- Exited Stream Mode. Returned to menu. ---\n";
            
        } else if (cmd == "clear") {
            kr = IOConnectCallMethod(
                connect,
                KEXTLOGKIT_USERCLIENT_SELECTOR_CLEAR,
                nullptr, 0,
                nullptr, 0,
                nullptr, nullptr,
                nullptr, nullptr
            );
            if (kr == KERN_SUCCESS) {
                std::cout << "Logs successfully cleared from kernel space.\n";
            } else {
                std::cerr << "Failed to clear logs, kr = 0x" << std::hex << kr << std::dec << "\n";
            }
            
        } else if (cmd == "readraw") {
            std::cout << "Select display format:\n [1] Plain Text (With Smooth Auto-Scroll)\n [2] Hex + ASCII Dump (Full Buffer Snap)\n> ";
            std::string subCmd;
            std::getline(std::cin, subCmd);

            const size_t rawCapacity = 1024 * 1024; // 分配 1MB 的接收安全承载区
            std::vector<char> rawBuffer(rawCapacity, 0);
            size_t outSize = rawCapacity;

            /* ==================== 自动刷新 Stream 模式 ==================== */
            if (subCmd == "1") {
                std::cout << "\033[?25l";
                std::cout << "\n--- Entering Raw Stream Mode. Press Ctrl+C to stop scrolling ---\n\n";
                
                size_t prevSize = 0;

                struct sigaction old_action;
                struct sigaction action;
                g_keep_stream_running = 1;
                
                action.sa_handler = [](int) { g_keep_stream_running = 0; };
                sigemptyset(&action.sa_mask);
                action.sa_flags = 0;
                sigaction(SIGINT, &action, &old_action);

                while (g_keep_stream_running) {
                    outSize = rawCapacity;
                    
                    kr = IOConnectCallMethod(
                        connect,
                        KEXTLOGKIT_USERCLIENT_SELECTOR_READALL_RAW,
                        nullptr, 0,
                        nullptr, 0,
                        nullptr, nullptr,
                        rawBuffer.data(),
                        &outSize
                    );

                    if (kr == KERN_SUCCESS && outSize > 0) {
                        if (prevSize == 0) {
                            std::cout.write(rawBuffer.data(), outSize);
                            std::flush(std::cout);
                        } else {
                            if (outSize >= prevSize) {
                                size_t delta = outSize - prevSize;
                                if (delta > 0) {
                                    std::cout.write(rawBuffer.data() + prevSize, delta);
                                    std::flush(std::cout);
                                }
                            } else {
                                std::cout << "\n\033[1;33m[Kernel Buffer Reset/Wrapped]\033[0m\n";
                                std::cout.write(rawBuffer.data(), outSize);
                                std::flush(std::cout);
                            }
                        }
                        prevSize = outSize;
                    }
                    usleep(100000);
                }

                sigaction(SIGINT, &old_action, nullptr);
                std::cout << "\033[?25h";
                std::cout << "\n--- Exited Raw Stream Mode. Returned to menu. ---\n";
            }
            /* ==================== Hex 盲读全区域 ==================== */
            else if (subCmd == "2") {
                outSize = rawCapacity;
                
                kr = IOConnectCallMethod(
                    connect,
                    KEXTLOGKIT_USERCLIENT_SELECTOR_READALL_RAW,
                    nullptr, 0,
                    nullptr, 0,
                    nullptr, nullptr,
                    rawBuffer.data(),
                    &outSize
                );
                
                if (kr == KERN_SUCCESS) {
                    if (outSize > 0) {
                        std::cout << "=== KEXT PHYSICAL BUFFER CORESNAP (" << outSize << " bytes) ===\n";
                        hexDump(rawBuffer.data(), outSize);
                        std::cout << "========================================================\n" << std::endl;
                    } else {
                        std::cout << "--- Kernel physical buffer is currently completely empty. ---\n";
                    }
                } else {
                    std::cerr << "Failed to capture raw buffer via Descriptor, kr = 0x" << std::hex << kr << std::dec << "\n";
                }
            } else {
                std::cout << "Cancelled.\n";
            }
        } else if (cmd == "exit") {
            running = false;
        } else {
            if (!cmd.empty() && (unsigned char)cmd[0] >= 0x20) {
                std::cout << "Unknown command: " << cmd << "\n";
            }
        }
    }

    IOServiceClose(connect);
    std::cout << "Disconnected.\n";

    return 0;
}
