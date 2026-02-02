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

#include <libretro-common/encodings/utf.h>

#include <borealis/core/box.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/platforms/desktop/steam_deck.hpp>
#include <borealis/platforms/glfw/glfw_ime.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/views/edit_text_dialog.hpp>
#include <borealis/views/label.hpp>
#include <codecvt>
#include <cstring>
#include <iostream>
#include <locale>

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

namespace brls
{
static int currentIMEStatus = GLFW_FALSE;
#define MAX_PREEDIT_LEN 128
static char preeditBuf[MAX_PREEDIT_LEN] = "";

static size_t encode_utf8(char* s, unsigned int ch)
{
    size_t count = 0;

    if (ch < 0x80)
        s[count++] = (char)ch;
    else if (ch < 0x800)
    {
        s[count++] = (ch >> 6) | 0xc0;
        s[count++] = (ch & 0x3f) | 0x80;
    }
    else if (ch < 0x10000)
    {
        s[count++] = (ch >> 12) | 0xe0;
        s[count++] = ((ch >> 6) & 0x3f) | 0x80;
        s[count++] = (ch & 0x3f) | 0x80;
    }
    else if (ch < 0x110000)
    {
        s[count++] = (ch >> 18) | 0xf0;
        s[count++] = ((ch >> 12) & 0x3f) | 0x80;
        s[count++] = ((ch >> 6) & 0x3f) | 0x80;
        s[count++] = (ch & 0x3f) | 0x80;
    }

    return count;
}

void GLFWImeManager::ime_callback(GLFWwindow* window)
{
    currentIMEStatus = glfwGetInputMode(window, GLFW_IME);
    brls::Logger::info("IME switched: {}", currentIMEStatus ? "ON" : "OFF");
}

void GLFWImeManager::preedit_callback(GLFWwindow* window, int preeditCount,
    unsigned int* preeditString, int blockCount,
    int* blockSizes, int focusedBlock, int caret)
{
    int blockIndex = -1, remainingBlockSize = 0;
    if (preeditCount == 0 || blockCount == 0)
    {
        strcpy(preeditBuf, "(empty)");
        preeditTextBuffer = "";
        return;
    }

    strcpy(preeditBuf, "");

    for (int i = 0; i < preeditCount; i++)
    {
        char encoded[5]     = "";
        size_t encodedCount = 0;

        if (i == caret)
        {
            if (strlen(preeditBuf) < MAX_PREEDIT_LEN)
                strcat(preeditBuf, "");
        }
        if (remainingBlockSize == 0)
        {
            if (blockIndex == focusedBlock)
            {
                if (strlen(preeditBuf) + strlen("]") < MAX_PREEDIT_LEN)
                    strcat(preeditBuf, "]");
            }
            blockIndex++;
            remainingBlockSize = blockSizes[blockIndex];
            if (blockIndex == focusedBlock)
            {
                if (strlen(preeditBuf) + strlen("[") < MAX_PREEDIT_LEN)
                    strcat(preeditBuf, "[");
            }
        }
        encodedCount          = encode_utf8(encoded, preeditString[i]);
        encoded[encodedCount] = '\0';
        if (strlen(preeditBuf) + strlen(encoded) < MAX_PREEDIT_LEN)
            strcat(preeditBuf, encoded);
        remainingBlockSize--;
    }
    if (blockIndex == focusedBlock)
    {
        if (strlen(preeditBuf) + strlen("]") < MAX_PREEDIT_LEN)
            strcat(preeditBuf, "]");
    }
    if (caret == preeditCount)
    {
        if (strlen(preeditBuf) < MAX_PREEDIT_LEN)
            strcat(preeditBuf, "");
    }

    preeditTextBuffer = std::string { preeditBuf };
}

void GLFWImeManager::char_callback(GLFWwindow* window, unsigned int codepoint)
{
    if (!showIME)
        return;
    if (cursor < 0 || cursor > (int)textBuffer.size())
        cursor = textBuffer.size();
    textBuffer.insert(textBuffer.begin() + cursor, (wchar_t)codepoint);
    cursor++;
}

GLFWImeManager::GLFWImeManager(GLFWwindow* window)
    : window(window)
{
    showIME          = false;
    currentIMEStatus = glfwGetInputMode(window, GLFW_IME);
    glfwSetPreeditCursorRectangle(window, 0, 0, 1, 1);
    glfwSetIMEStatusCallback(window, ime_callback);
    glfwSetPreeditCallback(window, preedit_callback);
    glfwSetCharCallback(window, char_callback);
}

void GLFWImeManager::openInputDialog(std::function<void(std::string)> cb, std::string headerText,
    std::string subText, size_t maxStringLength, std::string initialText)
{
#ifdef __linux__
    if (isSteamDeck())
    {
        brls::delay(200, []()
            { runSteamDeckCommand("steam://open/keyboard?Mode=0\n"); });
    }
#endif
    preeditTextBuffer.clear();
    glfwSetInputMode(window, GLFW_IME, GLFW_TRUE);
    showIME     = true;
    textBuffer  = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(initialText);
    auto dialog = new EditTextDialog();
    dialog->setText(initialText);
    cursor = -1;
    dialog->setCursor(cursor);
    dialog->setHintText(subText);
    dialog->setHeaderText(headerText);
    dialog->setCountText(std::to_string(utf8len(initialText.data())) + "/" + std::to_string(maxStringLength));
#if defined(BOREALIS_USE_D3D11)
    float scale = Application::windowScale;
#else
    float scale = Application::windowScale / Application::getPlatform()->getVideoContext()->getScaleFactor();
#endif
    dialog->getLayoutEvent()->subscribe([this, scale](Point p)
        { glfwSetPreeditCursorRectangle(window, p.x * scale, p.y * scale, 1, 1); });

    dialog->getClipboardEvent()->subscribe([dialog, maxStringLength](const std::string& str)
        {
            if(isEditing || str.empty()) return;
            if (cursor < 0 || cursor > (int)textBuffer.size()) cursor = textBuffer.size();
            auto left = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(textBuffer.substr(0, cursor));
            auto right = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(textBuffer.substr(cursor, textBuffer.size()));
            auto clipboard = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str);
            textBuffer.insert(textBuffer.begin() + cursor, clipboard.begin(), clipboard.end());
            cursor += clipboard.size();
            if (textBuffer.size() > maxStringLength) {
                textBuffer = textBuffer.substr(0, maxStringLength);
            }
            if (cursor < 0 || cursor > (int)textBuffer.size()) cursor = textBuffer.size();
            dialog->setText(std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(textBuffer));
            dialog->setCursor(cursor); });

    // update
    auto eventID = Application::getRunLoopEvent()->subscribe([dialog, maxStringLength]()
        {
            static std::wstring lastText = textBuffer;
            static std::string lastPreeditText = preeditTextBuffer;
            if(lastText != textBuffer){
                if(textBuffer.size() > maxStringLength)
                    textBuffer.erase(maxStringLength, textBuffer.size() - maxStringLength);
                lastText = textBuffer;
                if(textBuffer.empty()){
                    dialog->setText("");
                } else{
                    dialog->setText(getInputText());
                    dialog->setCursor(cursor);
                }
                dialog->setCountText(std::to_string(textBuffer.size()) + "/" + std::to_string(maxStringLength));
                lastPreeditText.clear();
                preeditTextBuffer.clear();
                isEditing = false;
            } else if(lastPreeditText != preeditTextBuffer){
                lastPreeditText = preeditTextBuffer;
                if (preeditTextBuffer.empty()) {
                    isEditing = false;
                    dialog->setText(getInputText());
                    dialog->setCursor(cursor);
                    return ;
                }
                isEditing = true;
                if (cursor < 0 || cursor > (int)textBuffer.size()) cursor = textBuffer.size();
                auto left = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(textBuffer.substr(0, cursor));
                auto right = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(textBuffer.substr(cursor, textBuffer.size()));
                dialog->setText(left + preeditTextBuffer + right);
                dialog->setCursor(cursor + preeditTextBuffer.size());
            } });

    // delete text
    dialog->getBackspaceEvent()->subscribe([dialog](...)
        {
            if(textBuffer.empty()) return true;
            if (cursor < 0 || cursor > (int)textBuffer.size()) cursor = textBuffer.size();
            if (cursor > 0 && cursor <= (int)textBuffer.size()) {
                textBuffer.erase(cursor - 1, 1);
                cursor--;
                dialog->setCursor(cursor);
            }
            return true; });

    dialog->registerAction(
        "hints/left"_i18n, BUTTON_LEFT, [dialog](...)
        {
            if (isEditing) return true;
            if (cursor == (int)CursorPosition::END) {
                cursor = textBuffer.empty() ? 0 : (int)textBuffer.size() - 1;
            } else if (cursor > (int)CursorPosition::START) {
                cursor--;
            }
            dialog->setCursor(cursor);
            return true; }, true, true);
    dialog->registerAction(
        "hints/right"_i18n, BUTTON_RIGHT, [dialog](...)
        {
            if (isEditing) return true;
            if (cursor >= (int)CursorPosition::START) {
                if (cursor < (int)textBuffer.size()) {
                    cursor++;
                    dialog->setCursor(cursor);
                }
            }
            return true; }, true, true);

    // cancel
    dialog->getCancelEvent()->subscribe([this, eventID]()
        {
            glfwSetInputMode(window, GLFW_IME, GLFW_FALSE);
            Application::getRunLoopEvent()->unsubscribe(eventID);
            showIME = false; });

    // submit
    dialog->getSubmitEvent()->subscribe([this, eventID, cb]()
        {
            glfwSetInputMode(window, GLFW_IME, GLFW_FALSE);
            Application::getRunLoopEvent()->unsubscribe(eventID);
            showIME = false;
            cb(getInputText());
            return true; });

    dialog->open();
}

std::string GLFWImeManager::getInputText()
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(textBuffer);
}

bool GLFWImeManager::openForText(std::function<void(std::string)> f, std::string headerText,
    std::string subText, int maxStringLength, std::string initialText,
    int kbdDisableBitmask)
{
    this->openInputDialog([f](const std::string& text)
        { f(text); },
        headerText, subText, maxStringLength, initialText);
    return true;
}

bool GLFWImeManager::openForNumber(std::function<void(long)> f, std::string headerText,
    std::string subText, int maxStringLength, std::string initialText,
    std::string leftButton, std::string rightButton,
    int kbdDisableBitmask)
{
    this->openInputDialog([f](const std::string& text)
        {
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
            } },
        headerText, subText, maxStringLength, initialText);
    return true;
}

};