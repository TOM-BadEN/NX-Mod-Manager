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
#ifdef __linux__

#include <borealis/core/logger.hpp>
#include <borealis/platforms/desktop/steam_deck.hpp>

namespace brls
{

#define STEAM_CLIENT_PIPE_PATH "/home/deck/.steam/steam.pipe"
#define STEAM_DECK_BACKLIGHT_PATH "/sys/class/backlight/amdgpu_bl0/brightness"
#define STEAM_DECK_MAX_BRIGHTNESS 4095

void runSteamDeckCommand(const std::string& cmd)
{
    if (!isSteamDeck())
        return ;
    int fd = open(STEAM_CLIENT_PIPE_PATH, O_WRONLY | O_NONBLOCK);
    if (fd < 0)
    {
        Logger::warning("Cannot open steam.pipe");
    }
    else
    {
        write(fd, cmd.c_str(), cmd.size());
        close(fd);
    }
}

bool isSteamDeck() {
    static bool isSteamDeck = getenv("SteamDeck");
    return isSteamDeck;
}

bool isSteamDeckBrightnessSupported() {
    if(!isSteamDeck()) return false;
    static bool tested = false, value = false;
    if (tested) return value;
    int fd = open(STEAM_DECK_BACKLIGHT_PATH, O_RDWR | O_NONBLOCK);
    if (fd < 0)
    {
        Logger::warning("Backlight control is not supported");
        tested = true;
        value = false;
        return false;
    }
    else
    {
        close(fd);
        Logger::info("Backlight control is supported");
        tested = true;
        value = true;
        return true;
    }
}

float getSteamDeckBrightness() {
    if(!isSteamDeck()) return 0.0f;
    int fd = open(STEAM_DECK_BACKLIGHT_PATH, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        Logger::warning("Backlight control is not supported");
        return 0.0f;
    }
    else
    {
        char brightness[16];
        read(fd, brightness, sizeof(brightness));
        close(fd);
        return atoi(brightness) * 1.0f / STEAM_DECK_MAX_BRIGHTNESS;
    }
}

void setSteamDeckBrightness(float value) {
    if(!isSteamDeck()) return;
    int fd = open(STEAM_DECK_BACKLIGHT_PATH, O_WRONLY | O_NONBLOCK);
    if (fd < 0)
    {
        Logger::warning("Backlight control is not supported");
    }
    else
    {
        int b = value * STEAM_DECK_MAX_BRIGHTNESS;
        std::string brightness = std::to_string(b);
        write(fd, brightness.c_str(), brightness.size());
        close(fd);
    }
}

}

#endif