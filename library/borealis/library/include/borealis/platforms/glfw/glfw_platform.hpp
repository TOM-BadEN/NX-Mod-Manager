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

#pragma once

#include <borealis/platforms/desktop/desktop_platform.hpp>
#include <borealis/platforms/glfw/glfw_ime.hpp>
#include <borealis/platforms/glfw/glfw_input.hpp>
#include <borealis/platforms/glfw/glfw_video.hpp>

namespace brls
{

class GLFWPlatform : public DesktopPlatform
{
  public:
    GLFWPlatform();
    ~GLFWPlatform() override;

    std::string getName() override;
    void createWindow(std::string windowTitle, uint32_t windowWidth, uint32_t windowHeight, float windowXPos, float windowYPos) override;

    void restoreWindow() override;
    void setWindowSize(uint32_t windowWidth, uint32_t windowHeight) override;
    void setWindowSizeLimits(uint32_t windowMinWidth, uint32_t windowMinHeight, uint32_t windowMaxWidth, uint32_t windowMaxHeight) override;
    void setWindowPosition(int windowXPos, int windowYPos) override;
    void setWindowState(uint32_t windowWidth, uint32_t windowHeight, int windowXPos, int windowYPos) override;

    void setWindowAlwaysOnTop(bool enable) override;
    void pasteToClipboard(const std::string& text) override;
    std::string pasteFromClipboard() override;
    bool mainLoopIteration() override;

    AudioPlayer* getAudioPlayer() override;
    VideoContext* getVideoContext() override;
    InputManager* getInputManager() override;
    ImeManager* getImeManager() override;

  private:
    NullAudioPlayer* audioPlayer   = nullptr;
    GLFWVideoContext* videoContext = nullptr;
    GLFWInputManager* inputManager = nullptr;
    GLFWImeManager* imeManager     = nullptr;
};

} // namespace brls
