/*
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

#include <SDL2/SDL.h>
#include <orbis/CommonDialog.h>
#include <orbis/NetCtl.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/SystemService.h>

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/platforms/ps4/ps4_platform.hpp>
#include <borealis/platforms/ps4/ps4_ime.hpp>
#ifdef USE_JBC
#include <libjbc.h>
#endif

extern "C" int sceSystemServiceLoadExec(const char* path, const char* args[]);

namespace brls
{

int (*sceRtcGetTick)(const OrbisDateTime* inOrbisDateTime, OrbisTick* outTick);
int (*sceRtcSetTick)(OrbisDateTime* outOrbisDateTime, const OrbisTick* inputTick);
int (*sceRtcConvertLocalTimeToUtc)(const OrbisTick* local_time, OrbisTick* utc);
int (*sceRtcConvertUtcToLocalTime)(const OrbisTick* utc, OrbisTick* local_time);
int (*sceRtcGetCurrentClockLocalTime)(OrbisDateTime* time);
int (*sceShellUIUtilLaunchByUri)(const char* uri, SceShellUIUtilLaunchByUriParam* param);
int (*sceShellUIUtilInitialize)();

int32_t sceTimezone;

#define GET_MODULE_SYMBOL(handle, symbol) moduleDlsym(handle, #symbol, reinterpret_cast<void**>(&symbol))
#ifdef USE_JBC
#define LOAD_SYS_MODULE(name) loadStartModule("/system/common/lib/" name)
#else
#define LOAD_SYS_MODULE(name) loadStartModuleFromSandbox(name)
#endif

Ps4Platform::Ps4Platform()
{
    // Initialize here for loading the system modules
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        Logger::error("sdl: failed to initialize video");
    }

#ifdef USE_JBC
    // Obtain root privileges after SDL initialization, as the sandbox path is used within SDL
    Ps4Platform::grantRootAccess();
#endif

    // NetCtl
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NETCTL) < 0 || sceNetCtlInit() < 0)
        brls::Logger::error("sceNetCtlInit() failed");

    // Dialogs
    if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG) < 0 || sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG) < 0)
        Logger::error("Load ime dialog module failed");
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG) < 0 || sceCommonDialogInitialize() < 0)
        brls::Logger::error("sceCommonDialogInitialize() failed");

    // SceRtc
    int handle = LOAD_SYS_MODULE("libSceRtc.sprx");
    GET_MODULE_SYMBOL(handle, sceRtcGetTick);
    GET_MODULE_SYMBOL(handle, sceRtcSetTick);
    GET_MODULE_SYMBOL(handle, sceRtcConvertLocalTimeToUtc);
    GET_MODULE_SYMBOL(handle, sceRtcConvertUtcToLocalTime);
    GET_MODULE_SYMBOL(handle, sceRtcGetCurrentClockLocalTime);

    // OpenBrowser
    handle = LOAD_SYS_MODULE("libSceShellUIUtil.sprx");
    GET_MODULE_SYMBOL(handle, sceShellUIUtilInitialize);
    GET_MODULE_SYMBOL(handle, sceShellUIUtilLaunchByUri);
    if (sceShellUIUtilInitialize && sceShellUIUtilInitialize() < 0)
        brls::Logger::error("sceShellUIUtilInitialize failed");

    int32_t ret = 0;

    // Locale
    if (Platform::APP_LOCALE_DEFAULT == LOCALE_AUTO || Platform::APP_LOCALE_DEFAULT.empty())
    {
        sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_LANG, &ret);
        switch (ret)
        {
            case ORBIS_SYSTEM_PARAM_LANG_CHINESE_S:
                this->locale = LOCALE_ZH_HANS;
                break;
            case ORBIS_SYSTEM_PARAM_LANG_CHINESE_T:
                this->locale = LOCALE_ZH_HANT;
                break;
            case ORBIS_SYSTEM_PARAM_LANG_JAPANESE:
                this->locale = LOCALE_JA;
                break;
            case ORBIS_SYSTEM_PARAM_LANG_KOREAN:
                this->locale = LOCALE_Ko;
                break;
            case ORBIS_SYSTEM_PARAM_LANG_ITALIAN:
                this->locale = LOCALE_IT;
                break;
            default:
                this->locale = LOCALE_DEFAULT;
        }
        brls::Logger::info("App locale: {}", this->locale);
    }

    // timezone
    sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_LANG, &sceTimezone);
    brls::Logger::info("System timezone: {}", sceTimezone);

    // swap a/b/x/y
    sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN, &ret);
    if (ret == ORBIS_SYSTEM_PARAM_ENTER_BUTTON_ASSIGN_CIRCLE)
        brls::Application::setSwapInputKeys(true);

    atexit([]()
        {
#ifdef USE_JBC
        Ps4Platform::exitRootAccess();
#endif
        sceSystemServiceLoadExec("exit", NULL); });
}

Ps4Platform::~Ps4Platform() = default;

std::string Ps4Platform::getName()
{
    return "Ps4";
}

void Ps4Platform::createWindow(std::string windowTitle, uint32_t windowWidth, uint32_t windowHeight, float windowXPos, float windowYPos)
{
    if (sceKernelIsNeoMode())
    {
        windowWidth  = 3840;
        windowHeight = 2160;
    }
    else
    {
        windowWidth  = 1920;
        windowHeight = 1080;
    }

    this->videoContext = new SDLVideoContext(windowTitle, windowWidth, windowHeight, NAN, NAN);
    this->inputManager = new SDLInputManager(this->videoContext->getSDLWindow());
    this->imeManager   = new Ps4ImeManager();
}

bool Ps4Platform::canShowWirelessLevel()
{
    return true;
}

bool Ps4Platform::hasWirelessConnection()
{
    OrbisNetCtlInfo info;
    int ret = sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS, &info);
    if (ret < 0)
        return false;
    ret = sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_SSID, &info);
    return ret >= 0;
}

int Ps4Platform::getWirelessLevel()
{
    OrbisNetCtlInfo info;
    int ret = sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_RSSI_PERCENTAGE, &info);
    if (ret < 0)
        return 0;
    if (info.rssi_percentage >= 75)
        return 3;
    else if (info.rssi_percentage >= 50)
        return 2;
    else if (info.rssi_percentage >= 25)
        return 1;
    return 0;
}

bool Ps4Platform::hasEthernetConnection()
{
    OrbisNetCtlInfo info;
    int ret = sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS, &info);
    if (ret < 0)
        return false;
    ret = sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_SSID, &info);
    return ret < 0;
}

std::string Ps4Platform::getIpAddress()
{
    OrbisNetCtlInfo info {};
    int ret = sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS, &info);
    if (ret < 0)
        return "-";
    return std::string { info.ip_address };
}

std::string Ps4Platform::getDnsServer()
{
    std::string dns = "-";
    OrbisNetCtlInfo info {};
    int ret = sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_PRIMARY_DNS, &info);
    if (ret < 0)
        return dns;
    dns = std::string { info.primary_dns };
    ret = sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_SECONDARY_DNS, &info);
    if (ret < 0)
        return dns;

    return dns + "\n" + std::string { info.secondary_dns };
}

void Ps4Platform::openBrowser(std::string url)
{
    SceShellUIUtilLaunchByUriParam param;
    param.size = sizeof(SceShellUIUtilLaunchByUriParam);
    sceUserServiceGetForegroundUser((int*)&param.userId);

    std::string launch_uri = std::string { "pswebbrowser:search?url=" } + url;
    int ret                = sceShellUIUtilLaunchByUri(launch_uri.c_str(), &param);
}

ImeManager* Ps4Platform::getImeManager()
{
    return this->imeManager;
}


int Ps4Platform::loadStartModuleFromSandbox(const std::string& name)
{
    std::string modulePath { sceKernelGetFsSandboxRandomWord() };
    modulePath = "/" + modulePath + "/common/lib/" + name;

    return loadStartModule(modulePath);
}

int Ps4Platform::loadStartModule(const std::string& path)
{
    int handle = sceKernelLoadStartModule(path.c_str(), 0, NULL, 0, NULL, NULL);
    if (handle == 0)
    {
        Logger::error("Failed to load module: {}", path);
    }
    return handle;
}

int Ps4Platform::moduleDlsym(int handle, const std::string& name, void** func)
{
    int ret = sceKernelDlsym(handle, name.c_str(), func);
    if (func == nullptr)
    {
        Logger::error("Failed to dlsym: {}", name);
    }
    return ret;
}

#ifdef USE_JBC
// Variables for (un)jailbreaking
static jbc_cred g_Cred;
static jbc_cred g_RootCreds;

void Ps4Platform::grantRootAccess()
{
    if (Ps4Platform::hasRootAccess())
    {
        brls::Logger::warning("Already has root access");
        return;
    }

    jbc_get_cred(&g_Cred);
    g_RootCreds = g_Cred;
    jbc_jailbreak_cred(&g_RootCreds);
    jbc_set_cred(&g_RootCreds);

    if (Ps4Platform::hasRootAccess())
    {
        brls::Logger::info("Root access granted");
    }
    else
    {
        brls::Logger::error("Failed to grant root access");
    }
}

void Ps4Platform::exitRootAccess()
{
    if (!Ps4Platform::hasRootAccess())
        return;

    jbc_set_cred(&g_Cred);
}

bool Ps4Platform::hasRootAccess()
{
    FILE* s_FilePointer = fopen("/user/.jailbreak", "w");

    if (!s_FilePointer)
        return false;

    fclose(s_FilePointer);
    remove("/user/.jailbreak");
    return true;
}
#endif

} // namespace brls
