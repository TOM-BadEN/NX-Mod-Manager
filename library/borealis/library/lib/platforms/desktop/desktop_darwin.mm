/*
    Copyright 2021 natinusala
    Copyright 2023 xfangfang

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <string>
#include <functional>

#include <AvailabilityMacros.h>

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
    #define HAS_CORE_WLAN 1
#else
    #define HAS_CORE_WLAN 0
#endif

#if HAS_CORE_WLAN
    #import <CoreWLAN/CoreWLAN.h>
#endif

namespace brls
{

// Interface method, fetching the current connection info.
int darwin_wlan_quality() {
#if HAS_CORE_WLAN
#ifdef __clang__
    @autoreleasepool {
        CWWiFiClient* Client = CWWiFiClient.sharedWiFiClient;
        CWInterface* currentInterface = Client.interface;
        if ([currentInterface powerOn] == false) {
            return -1;
        }
        if ([currentInterface serviceActive] == false) {
            return 0;
        }
        int rssi = [currentInterface rssiValue];
        if (rssi > -50) return 3;
        if (rssi > -80) return 2;
        return 1;
    }
#else
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    CWInterface* currentInterface = [CWInterface interface];
    if (![currentInterface ssid]) {
        [pool drain];
        return 0;
    }
    NSNumber* rssiNumber = [currentInterface rssi];
    int result = 1;
    if (rssiNumber) {
        int rssi = [rssiNumber intValue];
        if (rssi > -50) result = 3;
        else if (rssi > -80) result = 2;
    }
    [pool drain];
    return result;
#endif
#else // HAS_CORE_WLAN
    return -1;
#endif
}

bool darwin_runloop(const std::function<bool()>& runLoopImpl) {
#ifdef __clang__
    @autoreleasepool {
        return runLoopImpl();
    }
#else
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    bool result = runLoopImpl();
    [pool drain];
    return result;
#endif
}

std::string darwin_locale() {
#ifdef __clang__
    @autoreleasepool {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSArray *languages = [defaults objectForKey:@"AppleLanguages"];
        NSString *current = [languages objectAtIndex:0];
        return [current UTF8String];
    }
#else
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *languages = [defaults objectForKey:@"AppleLanguages"];
    NSString *current = [languages objectAtIndex:0];
    [pool drain];
    return [current UTF8String];
#endif
}

}
