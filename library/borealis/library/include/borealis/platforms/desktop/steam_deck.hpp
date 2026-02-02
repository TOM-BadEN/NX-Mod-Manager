/*
Copyright 2024 xfangfang

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

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>

#include <string>

namespace brls
{

/**
 * Call Steam client internal command
 * @param cmd example: steam://close/keyboard
 */
void runSteamDeckCommand(const std::string& cmd);

/**
 * return true if we are running in SteamDeck game mode
 */
bool isSteamDeck();

bool isSteamDeckBrightnessSupported();

float getSteamDeckBrightness();

void setSteamDeckBrightness(float value);

}
#endif