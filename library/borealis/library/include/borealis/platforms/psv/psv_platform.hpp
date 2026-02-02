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

#pragma once

#ifdef BOREALIS_USE_GXM
#include <borealis/platforms/desktop/desktop_platform.hpp>
#include <borealis/platforms/psv/psv_ime.hpp>
#include <borealis/platforms/psv/psv_input.hpp>
#include <borealis/platforms/psv/psv_video.hpp>
#else
#include <borealis/platforms/sdl/sdl_platform.hpp>
#endif

namespace brls
{

#ifdef BOREALIS_USE_GXM
class PsvPlatform : public DesktopPlatform
#else
class PsvPlatform : public SDLPlatform
#endif
{
  public:
    PsvPlatform();
    ~PsvPlatform() override;
    std::string getName() override;
    void createWindow(std::string windowTitle, uint32_t windowWidth, uint32_t windowHeight, float windowXPos, float windowYPos) override;
    bool canShowBatteryLevel() override;
    bool canShowWirelessLevel() override;
    int getBatteryLevel() override;
    bool isBatteryCharging() override;
    bool hasWirelessConnection() override;
    int getWirelessLevel() override;
    bool hasEthernetConnection() override;
    std::string getIpAddress() override;
    std::string getDnsServer() override;
    void openBrowser(std::string url) override;
    void setBacklightBrightness(float brightness) override;
    float getBacklightBrightness() override;
    bool canSetBacklightBrightness() override;
    bool isScreenDimmingDisabled() override;
    void disableScreenDimming(bool disable, const std::string& reason, const std::string& app) override;
    bool mainLoopIteration() override;
    InputManager* getInputManager() override;
    ImeManager* getImeManager() override;
    VideoContext* getVideoContext() override;
    AudioPlayer* getAudioPlayer() override;

  private:
    bool suspendDisabled{};
#ifdef BOREALIS_USE_GXM
    PsvInputManager* inputManager = nullptr;
    PsvImeManager* imeManager     = nullptr;
    PsvVideoContext* videoContext = nullptr;
    NullAudioPlayer* audioPlayer  = nullptr;
#endif
};

} // namespace brls
