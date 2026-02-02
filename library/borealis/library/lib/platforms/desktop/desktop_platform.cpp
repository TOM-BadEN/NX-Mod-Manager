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

#include <strings.h>

#include <borealis/core/i18n.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/platforms/desktop/desktop_platform.hpp>
#include <borealis/platforms/desktop/steam_deck.hpp>
#include <memory>
#include <sstream>

#ifdef __SDL2__
#include <SDL2/SDL_misc.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <wlanapi.h>
#include <shellapi.h>
#include <winioctl.h>
#include <ntddvdeo.h>
#elif IOS || TVOS
#elif __APPLE__
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>
#include <SystemConfiguration/SystemConfiguration.h>
#endif

#ifdef __WINRT__
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Networking.Connectivity.h>
#include <winrt/Windows.Devices.WiFi.h>
#include <winrt/Windows.UI.ViewManagement.h>
using winrt::Windows::Devices::WiFi::WiFiAdapter;
using winrt::Windows::UI::ViewManagement::UIColorType;
using winrt::Windows::UI::ViewManagement::UISettings;
#endif

#if defined(__APPLE__) || defined(__linux__)
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif


namespace brls
{
#ifdef __WINRT__

const static auto timeout = std::chrono::milliseconds(500);

int winrt_wlan_quality()
{
    auto async = WiFiAdapter::FindAllAdaptersAsync();
    auto code  = async.wait_for(timeout);
    if (code == winrt::Windows::Foundation::AsyncStatus::Completed)
    {
        auto adapters = async.GetResults();
        for (auto it : adapters)
        {
            auto profileAsync = it.NetworkAdapter().GetConnectedProfileAsync();
            auto profileCode  = profileAsync.wait_for(timeout);
            if (profileCode == winrt::Windows::Foundation::AsyncStatus::Completed)
            {
                auto profile = profileAsync.GetResults();
                if (profile != nullptr && profile.IsWlanConnectionProfile())
                {
                    return int(profile.GetNetworkConnectivityLevel());
                }
            }
        }
    }
    return -1; // No WiFi Adapter found.
}
#elif defined(_WIN32)
void shell_open(const char* command)
{
    std::vector<WCHAR> wcmd(strlen(command) + 1);
    MultiByteToWideChar(CP_UTF8, 0, command, -1, wcmd.data(), wcmd.size());
    ShellExecuteW(nullptr, L"open", wcmd.data(), nullptr, nullptr, SW_SHOWNORMAL);
}

/// @return -1 no wifi interface
///          0 no wifi connected
int win32_wlan_quality()
{
    HANDLE hwlan;
    DWORD version;
    int quality = -1;
    if (!WlanOpenHandle(2, nullptr, &version, &hwlan))
    {
        PWLAN_INTERFACE_INFO_LIST ppinfo = nullptr;
        if (!WlanEnumInterfaces(hwlan, nullptr, &ppinfo))
        {
            for (DWORD i = 0; i < ppinfo->dwNumberOfItems; i++)
            {
                if (ppinfo->InterfaceInfo[i].isState != wlan_interface_state_connected)
                {
                    continue;
                }
                PWLAN_CONNECTION_ATTRIBUTES attrs = nullptr;
                DWORD attr_size                   = sizeof(attrs);
                if (!WlanQueryInterface(hwlan, &ppinfo->InterfaceInfo[i].InterfaceGuid,
                        wlan_intf_opcode_current_connection, nullptr,
                        &attr_size, (PVOID*)&attrs, nullptr))
                {
                    quality = attrs->wlanAssociationAttributes.wlanSignalQuality;
                    WlanFreeMemory(attrs);
                    break;
                }
            }
            WlanFreeMemory(ppinfo);
        }
        WlanCloseHandle(hwlan, nullptr);
    }
    return quality;
}
#elif IOS || TVOS
extern ThemeVariant ios_theme();
extern uint8_t ios_battery_status();
extern float ios_battery();
extern bool darwin_runloop(const std::function<bool()>& runLoopImpl);
#elif __APPLE__
extern int darwin_wlan_quality();
extern bool darwin_runloop(const std::function<bool()>& runLoopImpl);

/// @return low 7bit is current capacity
int darwin_get_powerstate()
{
    CFTypeRef blob = IOPSCopyPowerSourcesInfo();
    if (!blob)
        return -1;

    CFArrayRef list = IOPSCopyPowerSourcesList(blob);
    if (!blob)
    {
        CFRelease(blob);
        return -1;
    }

    int capacity  = -1;
    CFIndex total = CFArrayGetCount(list);
    for (CFIndex i = 0; i < total; i++)
    {
        CFTypeRef ps         = (CFTypeRef)CFArrayGetValueAtIndex(list, i);
        CFDictionaryRef dict = IOPSGetPowerSourceDescription(blob, ps);
        if (!dict)
            continue;
        CFStringRef transport_type = (CFStringRef)CFDictionaryGetValue(dict, CFSTR(kIOPSTransportTypeKey));
        // Not a battery?
        if (transport_type && CFEqual(transport_type, CFSTR(kIOPSInternalType)))
        {
            CFStringRef psstat_ref    = (CFStringRef)CFDictionaryGetValue(dict, CFSTR(kIOPSPowerSourceStateKey));
            CFBooleanRef charging_ref = (CFBooleanRef)CFDictionaryGetValue(dict, CFSTR(kIOPSIsChargingKey));
            CFNumberRef capacity_ref  = (CFNumberRef)CFDictionaryGetValue(dict, CFSTR(kIOPSCurrentCapacityKey));
            if (capacity_ref != nullptr)
                CFNumberGetValue(capacity_ref, kCFNumberIntType, &capacity);
            if (charging_ref && CFBooleanGetValue(charging_ref))
                capacity |= 0x80;
            if (psstat_ref && CFEqual(psstat_ref, CFSTR(kIOPSACPowerValue)))
                capacity |= 0x80;
            break;
        }
    }
    CFRelease(list);
    CFRelease(blob);
    return capacity;
}
#elif __SDL2__
#elif defined(__linux__)
// Thanks to: https://github.com/videolan/vlc/blob/master/modules/misc/inhibit/dbus.c
enum INHIBIT_TYPE
{
    FDO_SS, /**< KDE >= 4 and GNOME >= 3.10 */
    FDO_PM, /**< KDE and GNOME <= 2.26 and Xfce */
    MATE, /**< >= 1.0 */
    GNOME, /**< GNOME 2.26..3.4 */
    NONE,
};

static const char dbus_service[][40] = {
    "org.freedesktop.ScreenSaver",
    "org.freedesktop.PowerManagement",
    "org.mate.SessionManager",
    "org.gnome.SessionManager",
};

static const char dbus_interface[][40] = {
    "org.freedesktop.ScreenSaver",
    "org.freedesktop.PowerManagement.Inhibit",
    "org.mate.SessionManager",
    "org.gnome.SessionManager",
};

static const char dbus_path[][41] = {
    "/ScreenSaver",
    "/org/freedesktop/PowerManagement/Inhibit",
    "/org/mate/SessionManager",
    "/org/gnome/SessionManager",
};

static const char dbus_method_uninhibit[][10] = {
    "UnInhibit",
    "UnInhibit",
    "Uninhibit",
    "Uninhibit",
};

static const char dbus_method_inhibit[] = "Inhibit";

static INHIBIT_TYPE systemType = NONE;

static inline void probeInhibitor(DBusConnection* conn)
{
    for (int i = FDO_SS; i < NONE; ++i)
    {
        if (dbus_bus_name_has_owner(conn, dbus_service[i], NULL))
        {
            Logger::info("Found inhibitor service: {}", dbus_service[i]);
            systemType = static_cast<INHIBIT_TYPE>(i);
            return;
        }
    }
    Logger::error("Failed to find an inhibitor service");
}

static DBusConnection* connectSessionBus()
{
    DBusConnection* bus;
    DBusError err;

    dbus_error_init(&err);

    bus = dbus_bus_get_private(DBUS_BUS_SESSION, &err);

    if (!bus)
    {
        Logger::error("Could not connect to bus: {}", err.message);
        dbus_error_free(&err);
    }

    return bus;
}

void closeSessionBus(DBusConnection* bus)
{
    Logger::info("DBus closed");
    dbus_connection_close(bus);
    dbus_connection_unref(bus);
}

uint32_t dbusInhibit(DBusConnection* connection, const std::string& app, const std::string& reason)
{
    if (systemType == NONE) {
        Logger::error("Idle inhibitor not available");
        return 0;
    }

    DBusMessage* msg = dbus_message_new_method_call(dbus_service[systemType],
        dbus_path[systemType],
        dbus_interface[systemType],
        dbus_method_inhibit);
    if (!msg)
    {
        Logger::error("DBus cannot create new method call: {};{};{};{}", dbus_service[systemType],
            dbus_path[systemType],
            dbus_interface[systemType],
            dbus_method_inhibit);
        return 0;
    }

    const char* app_ptr    = app.c_str();
    const char* reason_ptr = reason.c_str();
    switch (systemType)
    {
        case MATE:
        case GNOME:
        {
            dbus_uint32_t xid    = 0;
            dbus_uint32_t gflags = 0xC;
            dbus_message_append_args(msg,
                DBUS_TYPE_STRING, &app_ptr,
                DBUS_TYPE_UINT32, &xid,
                DBUS_TYPE_STRING, &reason_ptr,
                DBUS_TYPE_UINT32, &gflags,
                DBUS_TYPE_INVALID);
            break;
        }
        default:
            dbus_message_append_args(msg,
                DBUS_TYPE_STRING, &app_ptr,
                DBUS_TYPE_STRING, &reason_ptr,
                DBUS_TYPE_INVALID);
            break;
    }

    DBusError dbus_error;
    dbus_error_init(&dbus_error);
    DBusMessage* dbus_reply = dbus_connection_send_with_reply_and_block(connection, msg, DBUS_TIMEOUT_USE_DEFAULT,
        &dbus_error);
    dbus_message_unref(msg);
    if (!dbus_reply)
    {
        Logger::error("DBus connection failed: {}/{}", dbus_error.name, dbus_error.message);
        dbus_error_free(&dbus_error);
        return 0;
    }

    uint32_t id               = 0;
    dbus_bool_t dbus_args_res = dbus_message_get_args(dbus_reply, &dbus_error, DBUS_TYPE_UINT32, &id,
        DBUS_TYPE_INVALID);
    dbus_message_unref(dbus_reply);
    if (!dbus_args_res)
    {
        Logger::error("DBus cannot parse replay: {}/{}", dbus_error.name, dbus_error.message);
        dbus_error_free(&dbus_error);
        return 0;
    }
    return id;
}

void dbusUnInhibit(DBusConnection* connection, uint32_t cookie)
{
    if (systemType == NONE) {
        Logger::error("Idle inhibitor not available");
        return;
    }

    DBusMessage* msg = dbus_message_new_method_call(dbus_service[systemType],
        dbus_path[systemType],
        dbus_interface[systemType],
        dbus_method_uninhibit[systemType]);
    if (!msg)
    {
        Logger::error("DBus cannot create new method call: {};{};{};{}", dbus_service[systemType],
            dbus_path[systemType],
            dbus_interface[systemType],
            dbus_method_uninhibit[systemType]);
        return;
    }

    dbus_message_append_args(msg,
        DBUS_TYPE_UINT32, &cookie,
        DBUS_TYPE_INVALID);

    DBusError dbus_error;
    dbus_error_init(&dbus_error);
    DBusMessage* dbus_reply = dbus_connection_send_with_reply_and_block(connection, msg, DBUS_TIMEOUT_USE_DEFAULT,
        &dbus_error);
    dbus_message_unref(msg);
    if (!dbus_reply)
    {
        Logger::error("DBus connection failed: {}/{}", dbus_error.name, dbus_error.message);
        dbus_error_free(&dbus_error);
    }
}

static std::unique_ptr<DBusConnection, std::function<void(DBusConnection*)>> dbus_conn(connectSessionBus(),
    closeSessionBus);
#endif

DesktopPlatform::DesktopPlatform()
{
    // Theme
    char* themeEnv = getenv("BOREALIS_THEME");
    if (themeEnv == nullptr)
    {
#if defined(IOS) || defined(TVOS)
        this->themeVariant = ios_theme();
#elif __APPLE__
        CFPropertyListRef propertyList = CFPreferencesCopyValue(
            CFSTR("AppleInterfaceStyle"),
            kCFPreferencesAnyApplication,
            kCFPreferencesCurrentUser,
            kCFPreferencesAnyHost);
        if (propertyList)
        {
            if (CFEqual(propertyList, CFSTR("Dark")))
            {
                this->themeVariant = ThemeVariant::DARK;
                brls::Logger::info("Set app theme: Dark");
            }
            CFRelease(propertyList);
        }
#elif defined(__WINRT__)
        auto clr = UISettings().GetColorValue(UIColorType::Foreground);
        if (((5 * clr.G) + (2 * clr.R) + clr.B) > (8 * 128))
        {
            this->themeVariant = ThemeVariant::DARK;
            brls::Logger::info("Set app theme: Dark");
        }
#elif defined(_WIN32)
        HMODULE hUxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (hUxtheme)
        {
            auto fnSystemUseDarkMode = reinterpret_cast<BOOL(WINAPI*)()>(GetProcAddress(hUxtheme, MAKEINTRESOURCEA(138)));
            if (fnSystemUseDarkMode != nullptr && fnSystemUseDarkMode())
            {
                this->themeVariant = ThemeVariant::DARK;
                brls::Logger::info("Set app theme: Dark");
            }
            FreeLibrary(hUxtheme);
        }
#endif
    }
    else if (!strcasecmp(themeEnv, "DARK"))
    {
        this->themeVariant = ThemeVariant::DARK;
    }

    // Locale
    if (Platform::APP_LOCALE_DEFAULT == LOCALE_AUTO)
    {
        char* langEnv = getenv("BOREALIS_LANG");
        this->locale  = langEnv ? std::string(langEnv) : LOCALE_DEFAULT;
    }
    else
    {
        this->locale = Platform::APP_LOCALE_DEFAULT;
        brls::Logger::info("Set app locale: {}", this->locale);
    }

#if defined(__WINRT__)
#elif defined(_WIN32)
    this->hLCD = ::CreateFileA("\\\\.\\LCD", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
#endif

    // Platform impls
    this->fontLoader = new DesktopFontLoader();
    this->imeManager = new DesktopImeManager();

#if defined(__linux__) && !defined(__SDL2__)
    probeInhibitor(dbus_conn.get());
#endif
}

bool DesktopPlatform::canShowBatteryLevel()
{
#if defined(IOS) || defined(TVOS)
    return ios_battery_status() != 0;
#elif defined(__APPLE__)
    return darwin_get_powerstate() >= 0;
#elif defined(_WIN32)
    SYSTEM_POWER_STATUS status;
    if (!GetSystemPowerStatus(&status))
        return false;
    return !(status.BatteryFlag & BATTERY_FLAG_NO_BATTERY);
#else
    return false;
#endif
}

bool DesktopPlatform::canShowWirelessLevel()
{
#if defined(IOS) || defined(TVOS)
    return false;
#elif defined(__APPLE__)
    return true;
#elif defined(_WIN32)
    return true;
#else
    return false;
#endif
}

int DesktopPlatform::getBatteryLevel()
{
#if defined(IOS) || defined(TVOS)
    return ios_battery() * 100;
#elif defined(__APPLE__)
    return darwin_get_powerstate() & 0x7F;
#elif defined(_WIN32)
    SYSTEM_POWER_STATUS status;
    if (!GetSystemPowerStatus(&status))
        return 0;
    return status.BatteryLifePercent;
#else
    return 100;
#endif
}

bool DesktopPlatform::isBatteryCharging()
{
#if defined(IOS) || defined(TVOS)
    return ios_battery_status() == 2;
#elif defined(__APPLE__)
    return darwin_get_powerstate() & 0x80;
#elif defined(_WIN32)
    SYSTEM_POWER_STATUS status;
    if (!GetSystemPowerStatus(&status))
        return false;
    return (status.BatteryFlag & BATTERY_FLAG_CHARGING) != 0;
#else
    return false;
#endif
}

bool DesktopPlatform::hasWirelessConnection()
{
#if defined(IOS) || defined(TVOS)
    return false;
#elif defined(__APPLE__)
    return darwin_wlan_quality() > 0;
#elif defined(__WINRT__)
    return winrt_wlan_quality() > 0;
#elif defined(_WIN32)
    return win32_wlan_quality() > 0;
#else
    return true;
#endif
}

int DesktopPlatform::getWirelessLevel()
{
#if defined(IOS) || defined(TVOS)
    return 0;
#elif defined(__APPLE__)
    return darwin_wlan_quality();
#elif defined(__WINRT__)
    return winrt_wlan_quality();
#elif defined(_WIN32)
    return (win32_wlan_quality() * 4 - 1) / 100;
#elif defined(ANDROID)
    return 0;
#else
    return 0;
#endif
}

bool DesktopPlatform::hasEthernetConnection()
{
    bool has_eth = false;
#if defined(IOS) || defined(TVOS)
#elif defined(__APPLE__)
    SCDynamicStoreRef storeRef = SCDynamicStoreCreate(nullptr, CFSTR("FindCurrentInterface"), nullptr, nullptr);
    CFDictionaryRef globalRef  = (CFDictionaryRef)SCDynamicStoreCopyValue(storeRef, CFSTR("State:/Network/Global/IPv4"));
    CFTypeRef primaryIf        = nullptr;
    if (globalRef)
    {
        CFDictionaryGetValueIfPresent(globalRef, kSCDynamicStorePropNetPrimaryInterface, &primaryIf);
            if (primaryIf)
                CFRetain(primaryIf);
        CFRelease(globalRef);
    }
    CFRelease(storeRef);

    if (!primaryIf)
        return false;

    CFArrayRef iflist = SCNetworkInterfaceCopyAll();
    if (iflist)
    {
        CFIndex count = CFArrayGetCount(iflist);
        for (CFIndex i = 0; i < count; i++)
        {
            auto netif   = (SCNetworkInterfaceRef)CFArrayGetValueAtIndex(iflist, i);
            auto if_type = SCNetworkInterfaceGetInterfaceType(netif);
            auto if_name = SCNetworkInterfaceGetBSDName(netif);
            if (CFEqual(primaryIf, if_name))
            {
                has_eth = CFEqual(if_type, kSCNetworkInterfaceTypeEthernet);
            }
        }
        CFRelease(iflist);
    }
    CFRelease(primaryIf);
#elif defined(_WIN32)
    PIP_ADAPTER_ADDRESSES addrs = nullptr;
    ULONG outlen = sizeof(IP_ADAPTER_ADDRESSES), ret = 0, curindex = 0;
    HANDLE heap      = GetProcessHeap();
    SOCKADDR_IN addr = { .sin_family = AF_INET, .sin_addr = { { .S_addr = inet_addr("8.8.8.8") } } };
    GetBestInterfaceEx(reinterpret_cast<SOCKADDR*>(&addr), &curindex);

    do
    {
        if (addrs != nullptr)
            addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(HeapReAlloc(heap, HEAP_ZERO_MEMORY, addrs, outlen));
        else
            addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(HeapAlloc(heap, HEAP_ZERO_MEMORY, outlen));

        ret = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            nullptr, addrs, &outlen);
    } while (ret == ERROR_BUFFER_OVERFLOW);

    if (ret == NO_ERROR)
    {
        for (auto cur = addrs; cur != nullptr; cur = cur->Next)
        {
            if (cur->OperStatus == IfOperStatusUp && cur->IfIndex == curindex)
            {
                has_eth = cur->IfType == IF_TYPE_ETHERNET_CSMACD;
                break;
            }
        }
    }
    else
    {
        Logger::warning("GetAdaptersAddresses failed {}", ret);
    }
    HeapFree(heap, 0, addrs);
#elif defined(ANDROID)
    return has_eth;
#endif
    return has_eth;
}

void DesktopPlatform::disableScreenDimming(bool disable, const std::string& reason, const std::string& app)
{
    if (this->screenDimmingDisabled == disable)
        return;
    this->screenDimmingDisabled = disable;

    if (disable)
    {
#ifdef __SDL2__
#elif defined(__linux__)
        inhibitCookie = dbusInhibit(dbus_conn.get(), app, reason);
#elif __APPLE__
        std::string sleepReason           = app + " " + reason;
        CFStringRef reasonForActivity     = ::CFStringCreateWithCString(kCFAllocatorDefault, sleepReason.c_str(),
                kCFStringEncodingUTF8);
        [[maybe_unused]] IOReturn success = IOPMAssertionCreateWithName(
            kIOPMAssertionTypeNoDisplaySleep, kIOPMAssertionLevelOn,
            reasonForActivity, &assertionID);
#elif _WIN32
        SetThreadExecutionState(ES_DISPLAY_REQUIRED | ES_CONTINUOUS);
#endif
    }
    else
    {
#ifdef __SDL2__
#elif defined(__linux__)
        if (inhibitCookie != 0)
            dbusUnInhibit(dbus_conn.get(), inhibitCookie);
#elif __APPLE__
        IOPMAssertionRelease(assertionID);
#elif _WIN32
        SetThreadExecutionState(ES_CONTINUOUS);
#endif
    }
}

bool DesktopPlatform::runLoop(const std::function<bool()>& runLoopImpl) {
#if __APPLE__
    return darwin_runloop(runLoopImpl);
#else
    return runLoopImpl();
#endif
}

bool DesktopPlatform::isScreenDimmingDisabled()
{
    return this->screenDimmingDisabled;
}

void DesktopPlatform::setBacklightBrightness(float brightness)
{
#if defined(__WINRT__)
    (void)brightness;
#elif defined(_WIN32)
    DISPLAY_BRIGHTNESS db = {
        .ucDisplayPolicy = DISPLAYPOLICY_BOTH,
        .ucACBrightness = (UCHAR)std::floor(brightness * 100),
        .ucDCBrightness = (UCHAR)std::floor(brightness * 100),
    };
    DeviceIoControl(this->hLCD, IOCTL_VIDEO_SET_DISPLAY_BRIGHTNESS,
        &db, sizeof(db), NULL, 0, NULL, NULL);
#elif defined(__linux__)
    setSteamDeckBrightness(brightness);
#else
    (void)brightness;
#endif
}

float DesktopPlatform::getBacklightBrightness()
{
#if defined(__WINRT__)
    return 0.0f;
#elif defined(_WIN32)
    DISPLAY_BRIGHTNESS db;
    DeviceIoControl(this->hLCD, IOCTL_VIDEO_QUERY_DISPLAY_BRIGHTNESS,
        NULL, 0, &db, sizeof(db), NULL, NULL);
    return db.ucACBrightness / 100.0f;
#elif defined(__linux__)
    return getSteamDeckBrightness();
#else
    return 0.0f;
#endif
}

bool DesktopPlatform::canSetBacklightBrightness()
{
#if defined(__WINRT__)
#elif defined(_WIN32)
    UCHAR abLevels[256];
    DWORD bytesReturned = 0;
    DeviceIoControl(
        this->hLCD,
        IOCTL_VIDEO_QUERY_SUPPORTED_BRIGHTNESS,
        NULL,
        0,
        abLevels,
        sizeof(abLevels),
        &bytesReturned,
        NULL);

    for (DWORD i = 0; i < bytesReturned; i++) {
        if (abLevels[i]) return true;
    }
#elif defined(__linux__)
    return isSteamDeckBrightnessSupported();
#endif
    return false;
}

std::string DesktopPlatform::getIpAddress()
{
    std::string ipaddr = "-";
#if defined(ANDROID)
#elif defined(IOS) || defined(TVOS)
#elif defined(__APPLE__) || defined(__linux__)
    struct ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) == 0)
    {
        for (auto addr = interfaces; addr != nullptr; addr = addr->ifa_next)
        {
            if (!addr->ifa_addr) continue;
            if (addr->ifa_addr->sa_family == AF_INET)
            {
                ipaddr = inet_ntoa(reinterpret_cast<struct sockaddr_in*>(addr->ifa_addr)->sin_addr);
            }
        }
    }
    freeifaddrs(interfaces);
#elif defined(_WIN32)
    PIP_ADAPTER_ADDRESSES addrs = nullptr;
    ULONG outlen = sizeof(IP_ADAPTER_ADDRESSES), ret = 0, curindex = 0;
    HANDLE heap      = GetProcessHeap();
    SOCKADDR_IN addr = { .sin_family = AF_INET, .sin_addr = { { .S_addr = inet_addr("8.8.8.8") } } };
    GetBestInterfaceEx(reinterpret_cast<SOCKADDR*>(&addr), &curindex);

    do
    {
        if (addrs != nullptr)
            addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(HeapReAlloc(heap, HEAP_ZERO_MEMORY, addrs, outlen));
        else
            addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(HeapAlloc(heap, HEAP_ZERO_MEMORY, outlen));

        ret = GetAdaptersAddresses(AF_INET,
            GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            nullptr, addrs, &outlen);
    } while (ret == ERROR_BUFFER_OVERFLOW);

    if (ret == NO_ERROR)
    {
        for (auto cur = addrs; cur != nullptr; cur = cur->Next)
        {
            if (cur->OperStatus == IfOperStatusUp && cur->IfIndex == curindex)
            {
                for (auto addr = cur->FirstUnicastAddress; addr != nullptr; addr = addr->Next)
                {
                    auto addrin = reinterpret_cast<PSOCKADDR_IN>(addr->Address.lpSockaddr);
                    ipaddr      = inet_ntoa(addrin->sin_addr);
                }
                break;
            }
        }
    }
    else
    {
        Logger::warning("GetAdaptersAddresses failed {}", ret);
    }
    HeapFree(heap, 0, addrs);
#endif
    return ipaddr;
}

std::string DesktopPlatform::getDnsServer()
{
    std::string dnssvr = "-";
#if defined(IOS) || defined(TVOS)
#elif defined(__APPLE__)
    SCPreferencesRef prefsDNS = SCPreferencesCreate(nullptr, CFSTR("DNSSETTING"), nullptr);
    CFArrayRef services       = SCNetworkServiceCopyAll(prefsDNS);
    char buffer[INET6_ADDRSTRLEN];
    if (services)
    {
        CFIndex count = CFArrayGetCount(services);
        for (CFIndex i = 0; i < count; i++)
        {
            const SCNetworkServiceRef service = (const SCNetworkServiceRef)CFArrayGetValueAtIndex(services, i);
            CFStringRef interfaceServiceID    = SCNetworkServiceGetServiceID(service);
            CFStringRef primaryservicepath    = CFStringCreateWithFormat(NULL, NULL, CFSTR("State:/Network/Service/%@/DNS"), interfaceServiceID);
            SCDynamicStoreRef dynRef          = SCDynamicStoreCreate(kCFAllocatorSystemDefault, CFSTR("DNSSETTING"), NULL, NULL);
            CFPropertyListRef propList        = SCDynamicStoreCopyValue(dynRef, primaryservicepath);
            if (propList)
            {
                CFArrayRef addresses = (CFArrayRef)CFDictionaryGetValue((CFDictionaryRef)propList, CFSTR("ServerAddresses"));
                long addressesCount  = CFArrayGetCount(addresses);
                for (long j = 0; j < addressesCount; j++)
                {
                    CFStringRef address = (CFStringRef)CFArrayGetValueAtIndex(addresses, j);
                    if (address)
                    {
                        const char* dnsAddress = CFStringGetCStringPtr(address, kCFStringEncodingMacRomanian);
                        if (dnsAddress)
                            dnssvr = dnsAddress;
                        else if (CFStringGetCString(address, buffer, sizeof(buffer), kCFStringEncodingMacRomanian))
                            dnssvr = buffer;
                    }
                }
                CFRelease(propList);
            }
            CFRelease(dynRef);
            CFRelease(primaryservicepath);
        }
        CFRelease(services);
    }
    CFRelease(prefsDNS);
#elif defined(_WIN32)
    PFIXED_INFO info = nullptr;
    ULONG outlen = sizeof(FIXED_INFO), ret = 0;
    HANDLE heap = GetProcessHeap();
    do
    {
        if (info != nullptr)
            info = reinterpret_cast<PFIXED_INFO>(HeapReAlloc(heap, HEAP_ZERO_MEMORY, info, outlen));
        else
            info = reinterpret_cast<PFIXED_INFO>(HeapAlloc(heap, HEAP_ZERO_MEMORY, outlen));
        ret = GetNetworkParams(info, &outlen);
    } while (ret == ERROR_BUFFER_OVERFLOW);

    if (ret == NO_ERROR)
    {
        dnssvr = info->DnsServerList.IpAddress.String;
        Logger::debug("Host Name: {} DNS Servers: {}", info->HostName, dnssvr);
    }
    else
    {
        Logger::warning("GetNetworkParams failed {}", ret);
    }
    HeapFree(heap, 0, info);
#elif defined(ANDROID)
#elif defined(__linux__)
#endif
    return dnssvr;
}

std::string DesktopPlatform::exec(const char* cmd)
{
    std::stringstream ss;
#if defined(ANDROID)
#elif defined(IOS) || defined(TVOS)
#elif defined(__APPLE__) || defined(__linux__)
    FILE* pipe = popen(cmd, "r");
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        ss << buffer;
    }
    pclose(pipe);
#elif defined(_WIN32) and !defined(__WINRT__)
    PROCESS_INFORMATION pi;
    std::vector<WCHAR> cmdline(strlen(cmd) + 1);
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };
    MultiByteToWideChar(CP_UTF8, 0, cmd, -1, cmdline.data(), cmdline.size());

    STARTUPINFOW si = { sizeof(STARTUPINFOW) };
    HANDLE hStdread = nullptr;
    si.dwFlags      = STARTF_USESTDHANDLES;
    CreatePipe(&hStdread, &si.hStdOutput, &sa, 0);
    SetHandleInformation(hStdread, HANDLE_FLAG_INHERIT, 0);

    if (CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(si.hStdOutput);

        char buffer[128];
        DWORD readlen = 0;
        while (ReadFile(hStdread, buffer, sizeof(buffer), &readlen, nullptr) == TRUE && readlen > 0)
        {
            ss.write(buffer, readlen);
        }
    }

    CloseHandle(hStdread);
#endif
    // ss.seekp(-1, std::ios_base::end);
    return ss.str();
}

bool DesktopPlatform::isApplicationMode()
{
    return true;
}

void DesktopPlatform::exitToHomeMode(bool value)
{
    DesktopPlatform::RESTART_APP = !value;
}

void DesktopPlatform::forceEnableGamePlayRecording()
{
}

void DesktopPlatform::openBrowser(std::string url)
{
    brls::Logger::debug("open url: {}", url);
#if __SDL2__
    SDL_OpenURL(url.c_str());
#elif __APPLE__
    std::string cmd = "open \"" + url + "\"";
    system(cmd.c_str());
#elif defined(__linux__)
    if (isSteamDeck())
    {
        runSteamDeckCommand(fmt::format("steam://openurl/{}\n", url));
    } else
    {
        std::string cmd = "xdg-open \"" + url + "\"";
        system(cmd.c_str());
    }
#elif defined(_WIN32) and !defined(__WINRT__)
    shell_open(url.c_str());
#endif
}

std::string DesktopPlatform::getName()
{
    return "Desktop";
}

FontLoader* DesktopPlatform::getFontLoader()
{
    return this->fontLoader;
}

ImeManager* DesktopPlatform::getImeManager()
{
    return this->imeManager;
}

ThemeVariant DesktopPlatform::getThemeVariant()
{
    return this->themeVariant;
}

void DesktopPlatform::setThemeVariant(ThemeVariant theme)
{
    this->themeVariant = theme;
}

std::string DesktopPlatform::getLocale()
{
    return this->locale;
}

DesktopPlatform::~DesktopPlatform()
{
#if defined(__WINRT__)
#elif defined(_WIN32)
    if (this->hLCD) ::CloseHandle(this->hLCD);
#endif

    delete this->fontLoader;
    delete this->imeManager;
}

} // namespace brls
