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
#include <borealis/extern/libretro-common/retro_timers.h>
#include <orbis/UserService.h>
#include <orbis/_types/ime_dialog.h>

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/platforms/ps4/ps4_ime.hpp>

extern "C"
{
    int sceImeDialogInit(const OrbisImeDialogSetting* param, void* unk);
    OrbisDialogStatus sceImeDialogGetStatus();
    int sceImeDialogGetResult(OrbisDialogResult* result);
    int sceImeDialogTerm();
}

namespace brls
{

#define SCE_IME_DIALOG_MAX_TITLE_LENGTH	(128)
#define SCE_IME_DIALOG_MAX_TEXT_LENGTH	(512)

static uint16_t input_u16[SCE_IME_DIALOG_MAX_TEXT_LENGTH + 1];
static uint16_t title_u16[SCE_IME_DIALOG_MAX_TITLE_LENGTH];
static uint16_t placeholder_u16[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
static uint8_t input_u8[SCE_IME_DIALOG_MAX_TEXT_LENGTH];

static OrbisImeDialogSetting imeDialogSetting {};

static void utf16_to_utf8(const uint16_t* src, uint8_t* dst)
{
    int i;
    for (i = 0; src[i]; i++)
    {
        if ((src[i] & 0xFF80) == 0)
        {
            *(dst++) = src[i] & 0xFF;
        }
        else if ((src[i] & 0xF800) == 0)
        {
            *(dst++) = ((src[i] >> 6) & 0xFF) | 0xC0;
            *(dst++) = (src[i] & 0x3F) | 0x80;
        }
        else if ((src[i] & 0xFC00) == 0xD800 && (src[i + 1] & 0xFC00) == 0xDC00)
        {
            *(dst++) = (((src[i] + 64) >> 8) & 0x3) | 0xF0;
            *(dst++) = (((src[i] >> 2) + 16) & 0x3F) | 0x80;
            *(dst++) = ((src[i] >> 4) & 0x30) | 0x80 | ((src[i + 1] << 2) & 0xF);
            *(dst++) = (src[i + 1] & 0x3F) | 0x80;
            i += 1;
        }
        else
        {
            *(dst++) = ((src[i] >> 12) & 0xF) | 0xE0;
            *(dst++) = ((src[i] >> 6) & 0x3F) | 0x80;
            *(dst++) = (src[i] & 0x3F) | 0x80;
        }
    }

    *dst = '\0';
}

static void utf8_to_utf16(const uint8_t* src, uint16_t* dst)
{
    int i;
    for (i = 0; src[i];)
    {
        if ((src[i] & 0xE0) == 0xE0)
        {
            *(dst++) = ((src[i] & 0x0F) << 12) | ((src[i + 1] & 0x3F) << 6) | (src[i + 2] & 0x3F);
            i += 3;
        }
        else if ((src[i] & 0xC0) == 0xC0)
        {
            *(dst++) = ((src[i] & 0x1F) << 6) | (src[i + 1] & 0x3F);
            i += 2;
        }
        else
        {
            *(dst++) = src[i];
            i += 1;
        }
    }

    *dst = '\0';
}

void InitializeImeDialog(const std::string& title, const std::string& init, const std::string& placeholder, OrbisImeType type = OrbisImeType::ORBIS_TYPE_DEFAULT, int max_length = 32)
{
    memset(&input_u16, 0, sizeof(input_u16));
    memset(&title_u16, 0, sizeof(title_u16));
    memset(&placeholder_u16, 0, sizeof(placeholder_u16));

    utf8_to_utf16((uint8_t*)init.c_str(), input_u16);
    utf8_to_utf16((uint8_t*)title.c_str(), title_u16);
    utf8_to_utf16((uint8_t*)placeholder.c_str(), placeholder_u16);

    memset(&imeDialogSetting, 0, sizeof(OrbisImeDialogSetting));

    int UserID = 0;
    sceUserServiceGetInitialUser(&UserID);
    imeDialogSetting.userId              = UserID;
    imeDialogSetting.type                = type;
    imeDialogSetting.enterLabel          = OrbisButtonLabel::ORBIS_BUTTON_LABEL_DEFAULT;
    imeDialogSetting.inputMethod         = OrbisInput::ORBIS__DEFAULT;
    imeDialogSetting.maxTextLength       = max_length;
    imeDialogSetting.inputTextBuffer     = reinterpret_cast<wchar_t*>(input_u16);
    imeDialogSetting.title               = reinterpret_cast<wchar_t*>(title_u16);
    imeDialogSetting.placeholder         = reinterpret_cast<wchar_t*>(placeholder_u16);
    imeDialogSetting.posx                = 1000;
    imeDialogSetting.posy                = 1000;
    imeDialogSetting.horizontalAlignment = OrbisHAlignment::ORBIS_H_CENTER;
    imeDialogSetting.verticalAlignment   = OrbisVAlignment::ORBIS_V_VALIGN_BOTTOM;
}

int32_t ShowImeDialog(std::function<void(std::string)> f)
{
    int ret = 0;
    if ((ret = sceImeDialogInit(&imeDialogSetting, nullptr)) != 0)
    {
        Logger::error("sceImeDialogInit() failed with {}", ret);
        return ret;
    }

    while (true)
    {
        auto status = (int)sceImeDialogGetStatus();
        if (status == ORBIS_DIALOG_STATUS_RUNNING)
        {
            retro_sleep(500);
            continue;
        }

        if (status == ORBIS_DIALOG_STATUS_NONE)
        {
            brls::Logger::info("ORBIS_DIALOG_STATUS_NONE");
            break;
        }

        OrbisDialogResult result;
        memset(&result, 0, sizeof(OrbisDialogResult));
        sceImeDialogGetResult(&result);

        if (result.endstatus == ORBIS_DIALOG_CANCEL)
        {
            brls::Logger::info("ORBIS_DIALOG_CANCEL");
        }
        else if (result.endstatus == ORBIS_DIALOG_ABORD)
        {
            brls::Logger::info("ORBIS_DIALOG_ABORD");
        }
        else if (result.endstatus == ORBIS_DIALOG_OK)
        {
            utf16_to_utf8(input_u16, input_u8);
            f(std::string { (char*)input_u8 });
        }

        break;
    }

    sceImeDialogTerm();
    return 0;
}

bool Ps4ImeManager::openForText(std::function<void(std::string)> f, std::string headerText,
    std::string subText, int maxStringLength, std::string initialText,
    int kbdDisableBitmask)
{
    InitializeImeDialog(headerText, initialText, subText, ORBIS_TYPE_DEFAULT, maxStringLength);
    ShowImeDialog(f);
    return true;
}

bool Ps4ImeManager::openForNumber(std::function<void(long)> f, std::string headerText,
    std::string subText, int maxStringLength, std::string initialText,
    std::string leftButton, std::string rightButton,
    int kbdDisableBitmask)
{

    InitializeImeDialog(headerText, initialText, subText, ORBIS_TYPE_NUMBER, maxStringLength);
    ShowImeDialog([f](const std::string& text)
    {
        if(text.empty()) return ;
        try
        {
            f(stoll(text));
        }
        catch (const std::exception& e)
        {
            Logger::error("Could not parse input, did you enter a valid integer?");
        }
    });
    return true;
}

}