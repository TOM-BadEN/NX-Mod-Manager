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

#include <psp2/ime_dialog.h>

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/platforms/psv/psv_ime.hpp>
#include <iostream>
#include <string>
#include <utility>

namespace brls
{

static void utf16_to_utf8(const uint16_t* src, uint8_t* dst)
{
    int i;
    for (i = 0; src[i] != 0; i++)
    {
        if (!(src[i] & 0xFF80))
        {
            *(dst++) = src[i] & 0xFF;
        }
        else if (!(src[i] & 0xF800))
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

class PsvImeView : public View
{
  public:
    PsvImeView()
    {
        this->setFocusable(true);
        this->setHideHighlight(true);
        Application::blockInputs();
    }

    void setMaxStringLength(int length)
    {
        this->maxStringLength = length;
    }

    void setInitialText(const std::string& initialText)
    {
        utf8_to_utf16(reinterpret_cast<const uint8_t*>(initialText.c_str()), libime_initval);
    }

    void setHeaderText(const std::string& headerText)
    {
        utf8_to_utf16(reinterpret_cast<const uint8_t*>(headerText.c_str()), libime_title);
    }

    void setCallback(std::function<void(std::string)> f)
    {
        callback = std::move(f);
    }

    void setTextFieldType(SceImeType t)
    {
        type = t;
    }

    void frame(FrameContext* ctx) override
    {
        this->checkImeEvent();
    }

    bool openImeDialog()
    {
        SceImeDialogParam param;
        sceImeDialogParamInit(&param);

        param.supportedLanguages = 0;
        param.languagesForced    = SCE_FALSE;
        param.type               = type;
        param.option             = 0;
        param.textBoxMode        = SCE_IME_DIALOG_TEXTBOX_MODE_WITH_CLEAR;
        param.dialogMode         = SCE_IME_DIALOG_DIALOG_MODE_WITH_CANCEL;
        param.maxTextLength      = maxStringLength;
        param.title              = libime_title;
        param.initialText        = libime_initval;
        param.inputTextBuffer    = ime_buffer;

        SceInt32 res = sceImeDialogInit(&param);
        if (res < 0)
        {
            brls::Logger::error("Failed to init IME dialog: {}", res);
            return false;
        }
        running = true;
        return true;
    }

    void checkImeEvent()
    {
        if (!running)
            return;
        SceCommonDialogStatus dialogStatus = sceImeDialogGetStatus();
        if (dialogStatus != SCE_COMMON_DIALOG_STATUS_FINISHED)
            return;

        uint8_t utf8_buffer[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
        std::string text;
        SceImeDialogResult result;
        memset(&result, 0, sizeof(SceImeDialogResult));
        sceImeDialogGetResult(&result);
        if (result.button == SCE_IME_DIALOG_BUTTON_ENTER)
        {

            utf16_to_utf8(ime_buffer, utf8_buffer);
            text = std::string{ (const char*)utf8_buffer };
        }

        sceImeDialogTerm();
        Application::popActivity(TransitionAnimation::NONE, [this, text](){
            if (!text.empty() && callback)
                callback(text);
        });
        Application::unblockInputs();
        running = false;
    }

    void draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx) { }

  private:
    SceImeType type;
    SceWChar16 ime_buffer[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
    SceWChar16 libime_initval[SCE_IME_DIALOG_MAX_TEXT_LENGTH];
    SceWChar16 libime_title[SCE_IME_DIALOG_MAX_TITLE_LENGTH];

    bool running {};
    int maxStringLength { SCE_IME_DIALOG_MAX_TEXT_LENGTH };
    std::function<void(std::string)> callback {};
};

PsvImeManager::PsvImeManager()
    = default;

bool PsvImeManager::openForText(std::function<void(std::string)> f, std::string headerText,
    std::string subText, int maxStringLength, std::string initialText,
    int kbdDisableBitmask)
{
    bool ret;
    auto ime = new PsvImeView();
    ime->setTextFieldType(SCE_IME_TYPE_DEFAULT);
    ime->setCallback(f);
    ime->setHeaderText(headerText);
    ime->setInitialText(initialText);
    ime->setMaxStringLength(maxStringLength);
    ret = ime->openImeDialog();
    Application::pushActivity(new Activity(ime), TransitionAnimation::NONE);
    return ret;
}

bool PsvImeManager::openForNumber(std::function<void(long)> f, std::string headerText,
    std::string subText, int maxStringLength, std::string initialText,
    std::string leftButton, std::string rightButton,
    int kbdDisableBitmask)
{
    bool ret;
    auto ime = new PsvImeView();
    ime->setTextFieldType(SCE_IME_TYPE_NUMBER);
    ime->setCallback([f](const std::string& text) {
        if(text.empty()) return ;
        try
        {
            f(stoll(text));
        }
        catch (const std::invalid_argument& e)
        {
            Logger::error("Could not parse input, did you enter a valid integer? {}", e.what());
        }
        catch (const std::out_of_range& e) {
            Logger::error("Out of range: {}", e.what());
        }
        catch (const std::exception& e)
        {
            Logger::error("Unexpected error occurred: {}", e.what());
        }
    });
    ime->setHeaderText(headerText);
    ime->setInitialText(initialText);
    ime->setMaxStringLength(maxStringLength);
    ret = ime->openImeDialog();
    Application::pushActivity(new Activity(ime), TransitionAnimation::NONE);
    return ret;
}

};