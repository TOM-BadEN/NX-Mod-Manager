#pragma once

#include <switch.h>
#include <malloc.h>
#include <cstdint>

extern char* fake_heap_start;
extern char* fake_heap_end;

namespace deviceInfo {

    /** @brief 检查网络是否可用（基于实际连接状态，非硬件开关） */
    inline bool isNetworkAvailable() {
        NifmInternetConnectionType type;
        u32 strength;
        NifmInternetConnectionStatus status;
        return nifmGetInternetConnectionStatus(&type, &strength, &status) == 0;
    }

    /** @brief 堆内存使用情况 */
    struct HeapUsage {
        uint64_t usedMB;
        uint64_t totalMB;
    };

    /** @brief 获取堆内存使用情况（已用/总量，单位 MB） */
    inline HeapUsage getHeapUsage() {
        struct mallinfo mi = mallinfo();
        return {
            static_cast<uint64_t>(mi.uordblks) / (1024 * 1024),
            static_cast<uint64_t>(fake_heap_end - fake_heap_start) / (1024 * 1024)
        };
    }

} // namespace deviceInfo
