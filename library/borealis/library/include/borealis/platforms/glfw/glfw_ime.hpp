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

#include <borealis/core/ime.hpp>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace brls
{

class GLFWImeManager : public ImeManager
{
  public:
    GLFWImeManager(GLFWwindow* window);

    bool openForText(std::function<void(std::string)> f, std::string headerText = "",
        std::string subText = "", int maxStringLength = 32, std::string initialText = "",
        int kbdDisableBitmask = KeyboardKeyDisableBitmask::KEYBOARD_DISABLE_NONE) override;

    bool openForNumber(std::function<void(long)> f, std::string headerText = "",
        std::string subText = "", int maxStringLength = 18, std::string initialText = "",
        std::string leftButton = "", std::string rightButton = "",
        int kbdDisableBitmask = KeyboardKeyDisableBitmask::KEYBOARD_DISABLE_NONE) override;

    void openInputDialog(std::function<void(std::string)> cb, std::string headerText,
        std::string subText, size_t maxStringLength = 50, std::string initialText = "");

  private:
    GLFWwindow* window;
    inline static bool showIME;
    inline static std::wstring textBuffer;
    inline static std::string preeditTextBuffer;
    inline static int cursor{};
    inline static bool isEditing{};
    static void ime_callback(GLFWwindow* window);
    static void preedit_callback(GLFWwindow* window, int preeditCount, unsigned int* preeditString,
        int blockCount, int* blockSizes, int focusedBlock, int caret);
    static void char_callback(GLFWwindow* window, unsigned int codepoint);
    static std::string getInputText();
};

};