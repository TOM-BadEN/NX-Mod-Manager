/*
    Copyright 2019 p-sam
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
#include <borealis/core/logger.hpp>
#include <borealis/platforms/switch/switch_font.hpp>

namespace brls
{

void SwitchFontLoader::loadFonts()
{
    PlFontData font;
    Result rc;
    NVGcontext* vg = brls::Application::getNVGContext();

    brls::Logger::info("switch system locale: {}", brls::Application::getPlatform()->getLocale());

    // Standard
    rc = plGetSharedFontByType(&font, PlSharedFontType_Standard);
    if (R_SUCCEEDED(rc))
        Application::loadFontFromMemory(FONT_REGULAR, font.address, font.size, false);
    else
        Logger::error("switch: could not load Standard shared font: {:#x}", rc);

    // Simplified Chinese
    // custom Font
    if (access(USER_FONT_PATH.c_str(), F_OK) != -1)
    {
        brls::Logger::info("Load custom font: {}", USER_FONT_PATH);
        this->loadFontFromFile(FONT_CHINESE_SIMPLIFIED, USER_FONT_PATH);
    }
    else
    {
        brls::Logger::warning("Cannot find custom font, (Searched at: {})", USER_FONT_PATH);
        rc = plGetSharedFontByType(&font, PlSharedFontType_ChineseSimplified);
        if (R_SUCCEEDED(rc) && Application::loadFontFromMemory(FONT_CHINESE_SIMPLIFIED, font.address, font.size, false))
            nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_REGULAR));
        else
            Logger::error("switch: could not load Chinese Simplified shared font: {:#x}", rc);
    }

    // Simplified Chinese ext
    rc = plGetSharedFontByType(&font, PlSharedFontType_ExtChineseSimplified);
    if (R_SUCCEEDED(rc) && Application::loadFontFromMemory(FONT_CHINESE_SIMPLIFIED_EXT, font.address, font.size, false))
        nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_CHINESE_SIMPLIFIED_EXT));
    else
        Logger::error("switch: could not load Chinese Simplified Extended shared font: {:#x}", rc);

    // Traditional Chinese
    rc = plGetSharedFontByType(&font, PlSharedFontType_ChineseTraditional);
    if (R_SUCCEEDED(rc) && Application::loadFontFromMemory(FONT_CHINESE_TRADITIONAL, font.address, font.size, false))
        nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_CHINESE_TRADITIONAL));
    else
        Logger::error("switch: could not load Chinese Traditional shared font: {:#x}", rc);

    // Korean
    rc = plGetSharedFontByType(&font, PlSharedFontType_KO);
    if (R_SUCCEEDED(rc) && Application::loadFontFromMemory(FONT_KOREAN_REGULAR, font.address, font.size, false))
        nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_KOREAN_REGULAR));
    else
        Logger::error("switch: could not load Korean shared font: {:#x}", rc);

    // Extended (symbols)
    rc = plGetSharedFontByType(&font, PlSharedFontType_NintendoExt);
    if (R_SUCCEEDED(rc) && Application::loadFontFromMemory(FONT_SWITCH_ICONS, font.address, font.size, false))
        nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_SWITCH_ICONS));
    else
        Logger::error("switch: could not load Extented shared font: {:#x}", rc);

    // Material icons
    if (this->loadMaterialFromResources())
        nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont(FONT_MATERIAL_ICONS));
    else
        Logger::error("switch: could not load Material icons font from resources");

    // Load Emoji
    if (!USER_EMOJI_PATH.empty())
    {
        if (access(USER_EMOJI_PATH.c_str(), F_OK) != -1)
        {
            brls::Logger::info("Load emoji font: {}", USER_EMOJI_PATH);
            this->loadFontFromFile("emoji", USER_EMOJI_PATH);
            nvgAddFallbackFontId(vg, Application::getFont(FONT_CHINESE_SIMPLIFIED), Application::getFont("emoji"));
        }
        else
        {
            brls::Logger::warning("Cannot find custom emoji, (Searched at: {})", USER_EMOJI_PATH);
        }
    }
}

} // namespace brls
