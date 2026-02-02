#import <borealis/core/logger.hpp>
#import <borealis/platforms/desktop/desktop_platform.hpp>
#import <UIKit/UIKit.h>

namespace brls {

ThemeVariant ios_theme() {
    if (UIScreen.mainScreen.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark)
        return ThemeVariant::DARK;
    else
        return ThemeVariant::LIGHT;
}

bool darwin_runloop(const std::function<bool()>& runLoopImpl) {
    @autoreleasepool {
        return runLoopImpl();
    }
}

uint8_t ios_battery_status() {
#if defined(IOS)
    UIDevice.currentDevice.batteryMonitoringEnabled = true;
    return UIDevice.currentDevice.batteryState;
#else
    return 0;
#endif
}

float ios_battery() {
#if defined(IOS)
    return UIDevice.currentDevice.batteryLevel;
#else
    return 0;
#endif
}
};
