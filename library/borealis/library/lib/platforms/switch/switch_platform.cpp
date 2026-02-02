/*
    Copyright 2021 natinusala

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

#include <switch.h>

#include <borealis/core/application.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/platforms/switch/switch_platform.hpp>

#include "fmt/format.h"

extern "C" u32 __nx_applet_exit_mode;

namespace brls
{

static AppletHookCookie applet_hook_cookie;
static void on_applet_hook(AppletHookType hook, void* arg)
{
    SwitchPlatform* platform = (SwitchPlatform*)arg;
    switch (hook)
    {
        case AppletHookType_OnExitRequest:
            brls::Logger::info("AppletHookType_OnExitRequest");
            brls::Application::quit();
            break;
        case AppletHookType_OnFocusState:
            switch (appletGetFocusState())
            {
                case AppletFocusState_InFocus:
                    brls::Logger::info("AppletFocusState_InFocus");
                    brls::Application::getWindowFocusChangedEvent()->fire(true);
                    break;
                case AppletFocusState_OutOfFocus:
                case AppletFocusState_Background:
                    brls::Logger::info("AppletFocusState_OutOfFocus");
                    brls::Application::getWindowFocusChangedEvent()->fire(false);
                    break;
                default:
                    break;
            }
            break;
#ifdef BOREALIS_USE_DEKO3D
        case AppletHookType_OnOperationMode:
            ((SwitchVideoContext*)platform->getVideoContext())->appletCallback(hook);
            break;
#endif
        default:
            break;
    }
}

SwitchPlatform::SwitchPlatform()
{
    // Cache theme variant before video context init
    // The background color is created once in the "static" command list
    // executed every frame, so we need to know the background color
    // to add the clear command to that list.
    ColorSetId colorSetId;
    setsysGetColorSetId(&colorSetId);

    appletSetWirelessPriorityMode(AppletWirelessPriorityMode_OptimizedForWlan);

    if (colorSetId == ColorSetId_Dark)
        this->themeVariant = ThemeVariant::DARK;
    else
        this->themeVariant = ThemeVariant::LIGHT;

    Logger::info("switch system theme: {}", colorSetId ? "Dark" : "Light");

    // Get locale
    if (Platform::APP_LOCALE_DEFAULT == LOCALE_AUTO)
    {
        uint64_t languageCode = 0;
        Result rc             = setGetSystemLanguage(&languageCode);
        if (R_SUCCEEDED(rc))
        {
            char* languageName = (char*)&languageCode;
            this->locale       = std::string(languageName);
        }
        else
        {
            Logger::error("switch: unable to get system language (error {:#x}), using the default one: {}", rc, LOCALE_DEFAULT);
            this->locale = LOCALE_DEFAULT;
        }
    }
    else
    {
        this->locale = Platform::APP_LOCALE_DEFAULT;
    }

    // Init platform impls
    this->audioPlayer = new SwitchAudioPlayer();
    this->fontLoader  = new SwitchFontLoader();
    this->imeManager  = new SwitchImeManager();

    appletHook(&applet_hook_cookie, on_applet_hook, this);
    appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
}

void SwitchPlatform::createWindow(std::string windowTitle, uint32_t windowWidth, uint32_t windowHeight, float windowXPos, float windowYPos)
{
#ifdef BOREALIS_USE_DEKO3D
    this->videoContext = new SwitchVideoContext();
#elif defined(__SDL2__)
    this->videoContext = new SDLVideoContext(windowTitle, windowWidth, windowHeight, 0, 0);
#else
    this->videoContext = new GLFWVideoContext(windowTitle, windowWidth, windowHeight);
#endif

    // Reinitialise controllers with settings from borealis, not by GLFW or SDL
    this->inputManager = new SwitchInputManager();
}

bool SwitchPlatform::canShowBatteryLevel()
{
    return true;
}

int SwitchPlatform::getBatteryLevel()
{
    u32 charge;
    psmGetBatteryChargePercentage(&charge);
    return (int)charge;
}

bool SwitchPlatform::isBatteryCharging()
{
    PsmChargerType type;
    psmGetChargerType(&type);
    return type == PsmChargerType_EnoughPower || type == PsmChargerType_LowPower;
}

bool SwitchPlatform::hasWirelessConnection()
{
    bool res = false;
    nifmIsWirelessCommunicationEnabled(&res);
    return res;
}

bool SwitchPlatform::hasEthernetConnection()
{
    bool res = false;
    nifmIsEthernetCommunicationEnabled(&res);
    return res;
}

int SwitchPlatform::getWirelessLevel()
{
    NifmInternetConnectionType type;
    u32 wifiSignal;
    NifmInternetConnectionStatus status;
    nifmGetInternetConnectionStatus(&type, &wifiSignal, &status);
    return wifiSignal;
}

void SwitchPlatform::disableScreenDimming(bool disable, const std::string& reason, const std::string& app)
{
    appletSetMediaPlaybackState(disable);
}

bool SwitchPlatform::isScreenDimmingDisabled()
{
    Logger::error("Not support isScreenDimmingDisabled()");
    return false;
}

void SwitchPlatform::setBacklightBrightness(float brightness)
{
    if (brightness < 0) brightness = 0.0f;
    if (brightness > 1) brightness = 1.0f;
    lblSetCurrentBrightnessSetting(brightness);
}

float SwitchPlatform::getBacklightBrightness()
{
    float brightness = 0.0f;
    lblGetCurrentBrightnessSetting(&brightness);
    return brightness;
}

bool SwitchPlatform::canSetBacklightBrightness()
{
    return true;
}

std::string SwitchPlatform::getIpAddress()
{
    u32 ip;
    nifmGetCurrentIpAddress(&ip);
    return fmt::format("{}.{}.{}.{}", ip & 0xff, (ip & 0xff00) >> 8, (ip & 0xff0000) >> 16, (ip & 0xff000000) >> 24);
}

std::string SwitchPlatform::getDnsServer()
{
    u32 ip, mask, gateway, dns1, dns2;
    nifmGetCurrentIpConfigInfo(&ip, &mask, &gateway, &dns1, &dns2);
    std::string dns1_str = fmt::format("{}.{}.{}.{}", dns1 & 0xff, (dns1 & 0xff00) >> 8, (dns1 & 0xff0000) >> 16, (dns1 & 0xff000000) >> 24);
    std::string dns2_str = fmt::format("{}.{}.{}.{}", dns2 & 0xff, (dns2 & 0xff00) >> 8, (dns2 & 0xff0000) >> 16, (dns2 & 0xff000000) >> 24);
    return dns1_str + "\n" + dns2_str;
}

bool SwitchPlatform::isApplicationMode()
{
    AppletType at = appletGetAppletType();
    return at == AppletType_Application || at == AppletType_SystemApplication;
}

void SwitchPlatform::exitToHomeMode(bool value)
{
    __nx_applet_exit_mode = value;
}

void SwitchPlatform::forceEnableGamePlayRecording()
{
    appletInitializeGamePlayRecording();
}

void SwitchPlatform::openBrowser(std::string url)
{
    WebCommonConfig config;

    Result rc = webPageCreate(&config, url.c_str());
    if (R_SUCCEEDED(rc))
    {
        rc = webConfigSetWhitelist(&config, "^http*");
        if (R_SUCCEEDED(rc))
        {
            rc = webConfigShow(&config, NULL);
        }
    }
}

std::string SwitchPlatform::getName()
{
    return "Switch";
}

bool SwitchPlatform::mainLoopIteration()
{
    return appletMainLoop();
}

VideoContext* SwitchPlatform::getVideoContext()
{
    return this->videoContext;
}

std::string SwitchPlatform::getLocale()
{
    return this->locale;
}

AudioPlayer* SwitchPlatform::getAudioPlayer()
{
    return this->audioPlayer;
}

InputManager* SwitchPlatform::getInputManager()
{
    return this->inputManager;
}

FontLoader* SwitchPlatform::getFontLoader()
{
    return this->fontLoader;
}

ImeManager* SwitchPlatform::getImeManager()
{
    return this->imeManager;
}

ThemeVariant SwitchPlatform::getThemeVariant()
{
    return this->themeVariant;
}

void SwitchPlatform::setThemeVariant(ThemeVariant theme)
{
    this->themeVariant = theme;
#ifdef BOREALIS_USE_DEKO3D
    this->videoContext->recordStaticCommands();
#endif
}

SwitchPlatform::~SwitchPlatform()
{
    delete this->audioPlayer;
    delete this->inputManager;
    delete this->videoContext;
    delete this->imeManager;

    appletUnhook(&applet_hook_cookie);
}

} // namespace brls
