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

#include <psp2/apputil.h>
#include <psp2/avconfig.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/net/netctl.h>
#include <psp2/power.h>
#include <psp2/registrymgr.h>
#include <psp2/system_param.h>
#include <sys/unistd.h>

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/platforms/psv/psv_platform.hpp>

namespace brls
{

static int powerCallback(int notifyId, int notifyCount, int powerInfo, void* common)
{
    if ((powerInfo & SCE_POWER_CB_APP_RESUME) || (powerInfo & SCE_POWER_CB_APP_RESUMING))
    {
        brls::Application::getWindowFocusChangedEvent()->fire(true);
    }
    else if ((powerInfo & SCE_POWER_CB_BUTTON_PS_PRESS) || (powerInfo & SCE_POWER_CB_APP_SUSPEND) || (powerInfo & SCE_POWER_CB_SYSTEM_SUSPEND))
    {
        brls::Application::getWindowFocusChangedEvent()->fire(false);
    }
    return 0;
}

int CallbackThread(SceSize args, void* arg)
{
    int cbid = sceKernelCreateCallback("Power Callback", 0, powerCallback, NULL);
    scePowerRegisterCallback(cbid);

    for (;;)
    {
        sceKernelDelayThreadCB(10000000);
    }

    return 0;
}

PsvPlatform::PsvPlatform()
{
    // The main thread does not handle callbacks, so we need to make one to handle them.
    int thid = sceKernelCreateThread("callbackThread", CallbackThread, 0x10000100, 0x10000, 0, 0, NULL);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);

    SceAppUtilInitParam initParam;
    SceAppUtilBootParam bootParam;
    memset(&initParam, 0, sizeof(SceAppUtilInitParam));
    memset(&bootParam, 0, sizeof(SceAppUtilBootParam));
    sceAppUtilInit(&initParam, &bootParam);

    // Auto swap X and O buttons
    int enterButton;
    sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_ENTER_BUTTON, &enterButton);
    if (enterButton == SCE_SYSTEM_PARAM_ENTER_BUTTON_CIRCLE)
        brls::Application::setSwapInputKeys(true);

    // Override locale
    if (Platform::APP_LOCALE_DEFAULT == LOCALE_AUTO)
    {
        int systemLanguage;
        sceAppUtilSystemParamGetInt(SCE_SYSTEM_PARAM_ID_LANG, &systemLanguage);

        std::unordered_map<int, std::string> psv2brls = {
            { SCE_SYSTEM_PARAM_LANG_CHINESE_S, LOCALE_ZH_HANS },
            { SCE_SYSTEM_PARAM_LANG_CHINESE_T, LOCALE_ZH_HANT },
            { SCE_SYSTEM_PARAM_LANG_JAPANESE, LOCALE_JA },
            { SCE_SYSTEM_PARAM_LANG_KOREAN, LOCALE_Ko },
            { SCE_SYSTEM_PARAM_LANG_ITALIAN, LOCALE_IT }
        };

        if (psv2brls.count(systemLanguage) > 0) {
            this->locale = psv2brls[systemLanguage];
        } else {
            this->locale = LOCALE_EN_US;
        }
        brls::Logger::info("Set app locale: {}", this->locale);
    }

    // trigger internal network init in newlib
    gethostname(nullptr, 0);
}

PsvPlatform::~PsvPlatform()
{
#ifdef BOREALIS_USE_GXM
    delete this->audioPlayer;
    delete this->inputManager;
    delete this->imeManager;
    delete this->videoContext;
#endif
}

std::string PsvPlatform::getName()
{
    return "Psv";
}

void PsvPlatform::createWindow(std::string windowTitle, uint32_t windowWidth, uint32_t windowHeight, float windowXPos, float windowYPos)
{
#ifdef BOREALIS_USE_GXM
    this->videoContext = new PsvVideoContext();
    this->inputManager = new PsvInputManager();
    this->imeManager   = new PsvImeManager();
#else
    this->videoContext = new SDLVideoContext(windowTitle, windowWidth, windowHeight, windowXPos, windowYPos);
    this->inputManager = new SDLInputManager(this->videoContext->getSDLWindow());
    this->imeManager   = new SDLImeManager(&this->otherEvent);
#endif
    this->audioPlayer  = new NullAudioPlayer();
}

bool PsvPlatform::mainLoopIteration()
{
    if (this->suspendDisabled) {
        sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
    }
#ifdef __SDL2__
    return SDLPlatform::mainLoopIteration();
#else
    return true;
#endif
}

InputManager* PsvPlatform::getInputManager()
{
    return this->inputManager;
}

ImeManager* PsvPlatform::getImeManager()
{
    return this->imeManager;
}

VideoContext* PsvPlatform::getVideoContext()
{
    return this->videoContext;
}

AudioPlayer* PsvPlatform::getAudioPlayer()
{
    return this->audioPlayer;
}

bool PsvPlatform::isScreenDimmingDisabled()
{
    return suspendDisabled;
}
void PsvPlatform::disableScreenDimming(bool disable, const std::string& reason, const std::string& app)
{
    (void) reason;
    (void) app;
    suspendDisabled = disable;
}

bool PsvPlatform::canShowBatteryLevel()
{
    return true;
}

int PsvPlatform::getBatteryLevel()
{
    return scePowerGetBatteryLifePercent();
}

bool PsvPlatform::isBatteryCharging()
{
    return scePowerIsPowerOnline() == SCE_TRUE;
}

bool PsvPlatform::canShowWirelessLevel()
{
    return true;
}

bool PsvPlatform::hasWirelessConnection()
{
    SceNetCtlInfo info;
    int ret = sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info);
    return ret >= 0;
}

int PsvPlatform::getWirelessLevel()
{
    SceNetCtlInfo info;
    int ret = sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_RSSI_PERCENTAGE, &info);
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

bool PsvPlatform::hasEthernetConnection()
{
    return false;
}

std::string PsvPlatform::getIpAddress()
{
    SceNetCtlInfo info {};
    int ret = sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_IP_ADDRESS, &info);
    if (ret < 0)
        return "-";
    return std::string { info.ip_address };
}

std::string PsvPlatform::getDnsServer()
{
    std::string dns = "-";
    SceNetCtlInfo info {};
    int ret = sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_PRIMARY_DNS, &info);
    if (ret < 0)
        return dns;
    dns = std::string { info.primary_dns };
    ret = sceNetCtlInetGetInfo(SCE_NETCTL_INFO_GET_SECONDARY_DNS, &info);
    if (ret < 0)
        return dns;
    return dns + "\n" + std::string { info.secondary_dns };
}

void PsvPlatform::openBrowser(std::string url)
{
    if (url.empty())
        return;
    SceAppUtilWebBrowserParam browser_param;
    browser_param.str = url.c_str();
    browser_param.strlen = url.length();
    browser_param.launchMode = 1;
    if (sceAppUtilLaunchWebBrowser(&browser_param) == SCE_OK) {
        return;
    }
    brls::Logger::error("Failed to open browser in MODAL mode, trying NORMAL mode");
    browser_param.launchMode = 0;
    if (sceAppUtilLaunchWebBrowser(&browser_param) != SCE_OK) {
        brls::Logger::error("Failed to open browser in NORMAL mode");
    }
}

void PsvPlatform::setBacklightBrightness(float brightness)
{
    int value = 21 + (int)(brightness * 65515);
    sceAVConfigSetDisplayBrightness(value);
    sceRegMgrSetKeyInt("/CONFIG/DISPLAY", "brightness", value);
}

float PsvPlatform::getBacklightBrightness()
{
    int brightness = 0;
    sceRegMgrGetKeyInt("/CONFIG/DISPLAY", "brightness", &brightness);
    return ((float)brightness - 21.0f) / 65515.0f;
}

bool PsvPlatform::canSetBacklightBrightness()
{
    return true;
}

} // namespace brls
