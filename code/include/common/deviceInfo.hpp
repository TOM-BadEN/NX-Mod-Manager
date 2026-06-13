/**
 * DeviceInfo - 设备硬件信息（懒加载，单文件）
 * 首次访问时自动从硬件读取并缓存，后续零开销
 */

#pragma once

#include <cstdint>
#include <switch.h>

class DeviceInfo {
public:
    static uint64_t getDeviceId() {
        static uint64_t id = fetchFromSwitch();
        return id;
    }

private:
    static uint64_t fetchFromSwitch() {
        uint64_t value = 0;
        if (R_SUCCEEDED(setcalInitialize())) {
            setcalGetDeviceId(&value);
            setcalExit();
        }
        return value;
    }
};
