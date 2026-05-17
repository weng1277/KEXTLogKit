#  1.1配套cli


/*
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
#include <csignal> // 新增：用于优雅处理 Ctrl+C 信号

#define KEXTLOGKIT_USERCLIENT_SELECTOR_READALL      0
#define KEXTLOGKIT_USERCLIENT_SELECTOR_READSTREAM   1
#define KEXTLOGKIT_USERCLIENT_SELECTOR_CLEAR        2
#define KEXTLOGKIT_USERCLIENT_SELECTOR_READALL_RAW  3 // 完全盲读物理缓冲区 Selector

// 静态全局/局部变量，用于在信号处理函数中安全控制流式循环
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
        printf("   ");
        i++;
    }
    std::cout << "  " << buff << "\n";
}

int main() {
    mach_port_t masterPort;
    io_connect_t connect;
    kern_return_t kr;

    // 获取 I/O Kit master port
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if (kr != KERN_SUCCESS) {
        std::cerr << "Failed to get master port: " << kr << "\n";
        return 1;
    }

    // 匹配 KEXT 服务
    io_service_t service = IOServiceGetMatchingService(masterPort,
                                IOServiceMatching("KEXTLogKit"));
    if (!service) {
        std::cerr << "Failed to find KEXTLogKit service in IORegistry.\n";
        return 1;
    }

    // 打开用户客户端连接
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
            const size_t bufSize = 4096;
            char buffer[bufSize];
            size_t outSize = bufSize;

            kr = IOConnectCallStructMethod(connect,
                                           KEXTLOGKIT_USERCLIENT_SELECTOR_READALL,
                                           nullptr, 0,
                                           buffer, &outSize);
            
            if (kr == KERN_SUCCESS) {
                if (outSize > 0) {
                    std::cout << "=== All Logs (" << outSize << " bytes) ===\n";
                    std::cout.write(buffer, outSize);
                    std::cout << "\n========================\n";
                } else {
                    std::cout << "--- No log data available in buffer. ---\n";
                }
            } else {
                std::cerr << "Driver communication error (readall), kr = 0x" << std::hex << kr << std::dec << "\n";
            }
            
        } else if (cmd == "readstream") {
            const size_t bufSize = 1024;
            char buffer[bufSize];
            size_t outSize = bufSize;

            kr = IOConnectCallStructMethod(connect,
                                           KEXTLOGKIT_USERCLIENT_SELECTOR_READSTREAM,
                                           nullptr, 0,
                                           buffer, &outSize);
            
            if (kr == KERN_SUCCESS) {
                if (outSize > 0) {
                    std::cout << "=== Stream Logs (" << outSize << " bytes) ===\n";
                    std::cout.write(buffer, outSize);
                    std::cout << "\n=============================\n";
                } else {
                    std::cout << "--- No new stream logs. ---\n";
                }
            } else {
                std::cerr << "Driver communication error (readstream), kr = 0x" << std::hex << kr << std::dec << "\n";
            }
            
        } else if (cmd == "clear") {
            kr = IOConnectCallStructMethod(connect,
                                           KEXTLOGKIT_USERCLIENT_SELECTOR_CLEAR,
                                           nullptr, 0,
                                           nullptr, nullptr);
            if (kr == KERN_SUCCESS) {
                std::cout << "Logs successfully cleared from kernel space.\n";
            } else {
                std::cerr << "Failed to clear logs, kr = 0x" << std::hex << kr << std::dec << "\n";
            }
            
        } else if (cmd == "readraw") {
            std::cout << "Select display format:\n [1] Plain Text (With Smooth Auto-Scroll)\n [2] Hex + ASCII Dump\n> ";
            std::string subCmd;
            std::getline(std::cin, subCmd);

            const size_t rawCapacity = 4096;
            std::vector<char> rawBuffer(rawCapacity, 0);
            size_t outSize = rawCapacity;

            // ==================== 完美优化的自动刷新 Stream 模式 ====================
            if (subCmd == "1") {
                std::cout << "\033[?25l"; // 隐藏光标，避免高频刷新闪烁
                std::cout << "\n--- Entering Raw Stream Mode. Press Ctrl+C to stop scrolling ---\n\n";
                
                size_t prevSize = 0;

                // 临时接管 Ctrl+C (SIGINT) 信号
                struct sigaction old_action;
                struct sigaction action;
                g_keep_stream_running = 1; // 重置控制状态
                
                action.sa_handler = [](int) { g_keep_stream_running = 0; };
                sigemptyset(&action.sa_mask);
                action.sa_flags = 0;
                sigaction(SIGINT, &action, &old_action);

                while (g_keep_stream_running) {
                    outSize = rawCapacity;
                    kr = IOConnectCallStructMethod(connect,
                                                   KEXTLOGKIT_USERCLIENT_SELECTOR_READALL_RAW,
                                                   nullptr, 0,
                                                   rawBuffer.data(), &outSize);

                    if (kr == KERN_SUCCESS && outSize > 0) {
                        if (prevSize == 0) {
                            // 第一次进入时，全量打印当前已有日志，光标落在最底端
                            std::cout.write(rawBuffer.data(), outSize);
                            std::cout.flush();
                        } else {
                            if (outSize >= prevSize) {
                                size_t delta = outSize - prevSize;
                                if (delta > 0) {
                                    // 完美追加：只吐出增量字节，终端会自动自然向下平滑滚动
                                    std::cout.write(rawBuffer.data() + prevSize, delta);
                                    std::cout.flush();
                                }
                            } else {
                                // 环形缓冲区在底层发生了 Reset/Wrapped 回绕
                                std::cout << "\n\033[1;33m[Kernel Buffer Reset/Wrapped]\033[0m\n";
                                std::cout.write(rawBuffer.data(), outSize);
                                std::cout.flush();
                            }
                        }
                        prevSize = outSize;
                    }
                    usleep(100000); // 100ms 探测一次增量，响应非常灵敏
                }

                // 恢复原有的信号处理器，并重新显示光标
                sigaction(SIGINT, &old_action, nullptr);
                std::cout << "\033[?25h";
                std::cout << "\n--- Exited Raw Stream Mode. Returned to menu. ---\n";
            }
            // ====================================================================
            else if (subCmd == "2") {
                outSize = rawCapacity;
                kr = IOConnectCallStructMethod(connect,
                                               KEXTLOGKIT_USERCLIENT_SELECTOR_READALL_RAW,
                                               nullptr, 0,
                                               rawBuffer.data(), &outSize);
                
                if (kr == KERN_SUCCESS) {
                    if (outSize > 0) {
                        std::cout << "=== KEXT PHYSICAL BUFFER CORESNAP (" << outSize << " bytes) ===\n";
                        hexDump(rawBuffer.data(), outSize);
                        std::cout << "========================================================\n" << std::endl;
                    } else {
                        std::cout << "--- Kernel physical buffer is currently completely empty. ---\n";
                    }
                } else {
                    std::cerr << "Failed to capture raw buffer, kr = 0x" << std::hex << kr << std::dec << "\n";
                }
            } else {
                std::cout << "Cancelled.\n";
            }
        } else if (cmd == "exit") {
            running = false;
        } else {
            // 这里过滤了可能误触方向键带进来的杂乱终端控制符
            if (!cmd.empty() && (unsigned char)cmd[0] >= 0x20) {
                std::cout << "Unknown command: " << cmd << "\n";
            }
        }
    }

    IOServiceClose(connect);
    std::cout << "Disconnected.\n";

    return 0;
}
*/
