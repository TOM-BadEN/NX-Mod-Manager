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

#pragma once

#ifdef BOREALIS_USE_DEKO3D
#include <borealis/platforms/switch/switch_video.hpp>
#elif defined(__SDL2__)
#include <borealis/platforms/sdl/sdl_video.hpp>
#else
#include <borealis/platforms/glfw/glfw_video.hpp>
#endif
#include <borealis/core/platform.hpp>
#include <borealis/core/theme.hpp>
#include <borealis/platforms/switch/switch_audio.hpp>
#include <borealis/platforms/switch/switch_font.hpp>
#include <borealis/platforms/switch/switch_ime.hpp>
#include <borealis/platforms/switch/switch_input.hpp>

namespace brls
{

class SwitchPlatform : public Platform
{
  public:
    SwitchPlatform();
    ~SwitchPlatform() override;

    void createWindow(std::string windowTitle, uint32_t windowWidth, uint32_t windowHeight, float windowXPos, float windowYPos) override;

    std::string getName() override;

    bool mainLoopIteration() override;
    ThemeVariant getThemeVariant() override;
    void setThemeVariant(ThemeVariant theme) override;
    std::string getLocale() override;
    ImeManager* getImeManager() override;

    VideoContext* getVideoContext() override;
    AudioPlayer* getAudioPlayer() override;
    InputManager* getInputManager() override;
    FontLoader* getFontLoader() override;
    bool canShowBatteryLevel() override;
    int getBatteryLevel() override;
    bool isBatteryCharging() override;
    bool hasWirelessConnection() override;
    bool hasEthernetConnection() override;
    int getWirelessLevel() override;
    void disableScreenDimming(bool disable, const std::string& reason, const std::string& app) override;
    bool isScreenDimmingDisabled() override;
    void setBacklightBrightness(float brightness) override;
    float getBacklightBrightness() override;
    bool canSetBacklightBrightness() override;
    std::string getIpAddress() override;
    std::string getDnsServer() override;
    bool isApplicationMode() override;
    void exitToHomeMode(bool value) override;
    void forceEnableGamePlayRecording() override;
    void openBrowser(std::string url) override;

    void appletCallback(AppletHookType hookType);

  private:
    ThemeVariant themeVariant;
    std::string locale;

    SwitchAudioPlayer* audioPlayer;
    SwitchInputManager* inputManager;
    SwitchImeManager* imeManager;

#ifdef BOREALIS_USE_DEKO3D
    SwitchVideoContext* videoContext;
#elif defined(__SDL2__)
    SDLVideoContext* videoContext;
#else
    GLFWVideoContext* videoContext;
#endif
    SwitchFontLoader* fontLoader;
    AppletHookCookie applet_hook_cookie;
};

} // namespace brls
