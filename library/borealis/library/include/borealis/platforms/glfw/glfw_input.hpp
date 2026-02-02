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

#include <borealis/core/input.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace brls
{

// Input manager for GLFW gamepad and keyboard
class GLFWInputManager : public InputManager
{
  public:
    GLFWInputManager(GLFWwindow* window);

    short getControllersConnectedCount() override;

    void updateUnifiedControllerState(ControllerState* state) override;
    
    void updateControllerState(ControllerState* state, int controller) override;

    bool getKeyboardKeyState(BrlsKeyboardScancode state) override;

    void updateTouchStates(std::vector<RawTouchState>* states) override;

    void updateMouseStates(RawMouseState* state) override;

    void sendRumble(unsigned short controller, unsigned short lowFreqMotor, unsigned short highFreqMotor) override;

    void runloopStart() override;
    
    void setPointerLock(bool lock) override;

  private:
    Point scrollOffset;
    Point pointerOffset;
    Point pointerOffsetBuffer;
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void cursorCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    GLFWwindow* window;
    bool pointerLocked = false;
};

};
