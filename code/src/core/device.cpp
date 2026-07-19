/**
 * Device - Switch 设备信息与控制能力封装
 */

#include "core/device.hpp"
#include "common/settings.hpp"

#include <malloc.h>
#include <switch.h>

extern AppletType __nx_applet_type;  // libnx 当前 Applet 类型
extern char* fake_heap_start;        // libnx fake heap 起始地址
extern char* fake_heap_end;          // libnx fake heap 结束地址

namespace deviceInfo::Memory {

HeapUsage getHeapUsage() {
    struct mallinfo mi = mallinfo();
    return {
        static_cast<uint64_t>(mi.uordblks) / (1024 * 1024),
        static_cast<uint64_t>(fake_heap_end - fake_heap_start) / (1024 * 1024)
    };
}

} // namespace deviceInfo::Memory

namespace deviceInfo::Network {

bool isAvailable() {
    NifmInternetConnectionType type;
    u32 strength;
    NifmInternetConnectionStatus status;
    return nifmGetInternetConnectionStatus(&type, &strength, &status) == 0;
}

} // namespace deviceInfo::Network

namespace deviceInfo::Identity {

static uint64_t fetchFromSwitch() {
    uint64_t value = 0;
    if (R_SUCCEEDED(setcalInitialize())) {
        setcalGetDeviceId(&value);
        setcalExit();
    }
    return value;
}

uint64_t getDeviceId() {
    static uint64_t id = fetchFromSwitch();
    return id;
}

} // namespace deviceInfo::Identity

namespace deviceControl::HomeButton {

void disable() {
    if (__nx_applet_type == AppletType_Application) {
        appletBeginBlockingHomeButtonShortAndLongPressed(0);
    } else {
        hiddbgInitialize();
        hidsysInitialize();
        hiddbgDeactivateHomeButton();
    }
}

void enable() {
    if (__nx_applet_type == AppletType_Application) {
        appletEndBlockingHomeButtonShortAndLongPressed();
    } else {
        hidsysActivateHomeButton();
        hidsysExit();
        hiddbgExit();
    }
}

} // namespace deviceControl::HomeButton

namespace deviceControl::CpuBoost {

void enableFastLoad() {
    if (!Settings::getBool("Performance", "cpuBoost", true)) return;
    appletSetCpuBoostMode(ApmCpuBoostMode_FastLoad);
}

void disable() {
    if (!Settings::getBool("Performance", "cpuBoost", true)) return;
    appletSetCpuBoostMode(ApmCpuBoostMode_Normal);
}

} // namespace deviceControl::CpuBoost
