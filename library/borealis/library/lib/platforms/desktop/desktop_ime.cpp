/*
    Copyright 2019  WerWolv
    Copyright 2019  p-sam
    Copyright 2023  xfangfang

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

#include <borealis/core/logger.hpp>
#include <borealis/platforms/desktop/desktop_ime.hpp>
#include <cstring>
#include <iostream>

namespace brls
{

static std::string terminalInput(std::string text)
{
    printf("\033[0;94m[INPUT] \033[0;36m%s\033[0m: ", text.c_str());
    std::string line;
    std::getline(std::cin, line);
    return line;
}

bool DesktopImeManager::openForText(std::function<void(std::string)> f, std::string headerText,
    std::string subText, int maxStringLength, std::string initialText,
    int kbdDisableBitmask)
{
    std::string line = terminalInput(headerText);
    f(line);
    return true;
}

bool DesktopImeManager::openForNumber(std::function<void(long)> f, std::string headerText,
    std::string subText, int maxStringLength, std::string initialText,
    std::string leftButton, std::string rightButton,
    int kbdDisableBitmask)
{
    std::string line = terminalInput(headerText);

    try
    {
        f(stoll(line));
        return true;
    }
    catch (const std::exception& e)
    {
        Logger::error("Could not parse input, did you enter a valid integer?");
        return false;
    }
}

};