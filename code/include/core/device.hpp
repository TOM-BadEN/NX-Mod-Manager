/**
 * Device - Switch 设备信息与控制能力封装
 */

#pragma once

#include <cstdint>

namespace deviceInfo {

namespace Memory {

    /** @brief Switch 堆内存使用情况 */
    struct HeapUsage {
        uint64_t usedMB;   // 已用堆内存（MB）
        uint64_t totalMB;  // 总堆内存（MB）
    };

    /**
     * @brief 获取堆内存使用情况
     * @return 已用和总堆内存，单位 MB
     */
    HeapUsage getHeapUsage();
}

namespace Network {
    /**
     * @brief 检查网络是否可用（基于实际连接状态，非硬件开关）
     * @return 网络是否可用
     */
    bool isAvailable();
}

namespace Identity {
    /**
     * @brief 获取设备 ID
     * @return Switch 设备 ID，读取失败时返回 0
     */
    uint64_t getDeviceId();
}

} // namespace deviceInfo

namespace deviceControl {

namespace HomeButton {
    /** @brief 临时禁用 Home 键 */
    void disable();

    /** @brief 恢复 Home 键 */
    void enable();
}

namespace CpuBoost {
    /** @brief 开启 FastLoad CPU 加速模式 */
    void enableFastLoad();

    /** @brief 恢复普通 CPU 模式 */
    void disable();
}

} // namespace deviceControl
