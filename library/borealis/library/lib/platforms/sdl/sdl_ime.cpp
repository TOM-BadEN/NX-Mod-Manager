/*
Copyright 2023 zeromake

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

#include <borealis/platforms/sdl/sdl_ime.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/event.hpp>
#include <borealis/views/edit_text_dialog.hpp>
#include <borealis/core/application.hpp>

#ifdef __PSV__
extern "C" uint8_t * vita_ime_init_text;
__attribute__((weak)) uint8_t* vita_ime_init_text = nullptr;

extern "C" uint32_t vita_ime_max_text;
__attribute__((weak)) uint32_t vita_ime_max_text = 32;

extern "C" uint32_t vita_ime_type;
__attribute__((weak)) uint32_t vita_ime_type = 0;
#endif

namespace brls
{
    SDLImeManager::SDLImeManager(Event<SDL_Event*> *event):
    event(event),
    cursor(-1){}

    static int utf8_len(std::string &s) {
        int result = 0;
        for (auto &it: s) {
            if ((it & 0xc0) != 0x80) {
                result += 1;
            }
        }
        return result;
    }

    static int utf8_find_prev(std::string &s, int size) {
        int result = 0;
        for (int i = s.size() - 1; i >= 0; i--) {
            char p = s.at(i);
            result += 1;
            if ((p & 0xc0) != 0x80) {
                size--;
            }
            if (size <= 0) {
                break;
            }
        }
        return result;
    }

    static int utf8_find_next(std::string &s, int offset, int size) {
        int result = 0;
        if (size == 0) {
            return 0;
        }
        for (size_t i = offset; i < s.size(); i++) {
            char p = s.at(i);
            if ((p & 0xc0) != 0x80) {
                size--;
            }
            if (size < 0) {
                break;
            }
            result += 1;
        }
        return result;
    }

    void SDLImeManager::openInputDialog(
        std::function<void(std::string)> cb,
        std::string headerText,
        std::string subText,
        size_t maxStringLength,
        std::string initialText) {
        EditTextDialog* dialog = new EditTextDialog();
        this->inputBuffer = initialText;
#ifdef __PSV__
        vita_ime_init_text = reinterpret_cast<uint8_t *>(const_cast<char *>(this->inputBuffer.c_str()));
        vita_ime_max_text = maxStringLength;
#endif
        auto updateText = [this, dialog, maxStringLength]() {
            std::string text = this->inputBuffer;
            if (this->editingBuffer.size() > 0) {
                std::string editing = fmt::format("[{}]", this->editingBuffer);
                if (this->cursor >= 0) {
                    int start = utf8_find_next(this->inputBuffer, 0, this->cursor);
                    text.insert(start, editing);
                } else {
                    text += editing;
                }
            }
            dialog->setText(text);
            dialog->setCountText(fmt::format("{}/{}", utf8_len(this->inputBuffer), maxStringLength));
        };
        auto updateTextCursor = [this, dialog]() {
            int cursor = this->cursor;
            if (cursor >= (int)CursorPosition::START && this->editingBuffer.size() > 0) {
                cursor += utf8_len(this->editingBuffer) + 2;
            }
            dialog->setCursor(cursor);
        };
        auto updateTextAndCursor = [this, updateText, updateTextCursor, maxStringLength](std::string text) {
            int prev_n = utf8_len(this->inputBuffer);
            if (prev_n >= maxStringLength) {
                return;
            }
            int n = utf8_len(text);
            if (prev_n + n > maxStringLength) {
                n = maxStringLength - prev_n;
                int end = utf8_find_next(text, 0, n);
                text = text.substr(0, end);
            }
            if (this->cursor >= 0) {
                int start = utf8_find_next(this->inputBuffer, 0, this->cursor);
                this->inputBuffer.insert(start, text);
                this->cursor += n;
            } else {
                this->inputBuffer += text;
            }
            updateTextCursor();
            updateText();
        };
        dialog->setHeaderText(headerText);
        dialog->setHintText(subText);
        cursor = -1;
        updateText();
        if(!initialText.empty()) updateTextCursor();
#if defined(BOREALIS_USE_D3D11)
        float scale = Application::windowScale;
 #else
        float scale = Application::windowScale / Application::getPlatform()->getVideoContext()->getScaleFactor();
#endif
        // 更新输入法条位置
        dialog->getLayoutEvent()->subscribe([scale](Point p) {
#ifndef PS4
            const SDL_Rect rect = {(int)(p.x* scale), (int)(p.y * scale), 100, 20};
            SDL_SetTextInputRect(&rect);
#endif
        });

        dialog->getClipboardEvent()->subscribe([this, updateTextAndCursor](const std::string& str)
            {
                if(this->isEditing || str.empty()) return;
                updateTextAndCursor(str);
            });

        auto eventID1 = event->subscribe([this, updateTextAndCursor, updateText, updateTextCursor](SDL_Event *e) {
            switch (e->type) {
            case SDL_TEXTINPUT:
                this->isEditing = false;
                updateTextAndCursor(e->text.text);
                break;
            case SDL_TEXTEDITING:
                if (strlen(e->edit.text) == 0) {
                    this->isEditing = false;
                } else {
                    this->isEditing = true;
                }
                this->editingBuffer = e->edit.text;
                updateTextCursor();
                updateText();
                break;
            }
        });
        
        dialog->registerAction(
            "hints/left"_i18n, BUTTON_LEFT, [this, updateTextCursor](...){
                if (this->isEditing) return true;
                if (this->cursor == (int)CursorPosition::END) {
                    this->cursor = utf8_len(this->inputBuffer) - 1;
                    if(this->cursor < 0) this->cursor = 0;
                    updateTextCursor();
                } else if (this->cursor > (int)CursorPosition::START) {
                    this->cursor--;
                    updateTextCursor();
                }
                return true;
            }, true, true
        );
        dialog->registerAction(
            "hints/right"_i18n, BUTTON_RIGHT, [this, updateTextCursor](...){
                if (this->isEditing) return true;
                if (this->cursor >= (int)CursorPosition::START) {
                    if (this->cursor < utf8_len(this->inputBuffer)) {
                        this->cursor++;
                        updateTextCursor();
                    }
                }
                return true;
            }, true, true
        );

        // delete text
        dialog->getBackspaceEvent()->subscribe([this, updateText, updateTextCursor](...) {
            if(inputBuffer.empty()) return true;
            if (this->cursor == (int)CursorPosition::START) return true;
            if (this->cursor > (int)CursorPosition::START) {
                int start = utf8_find_next(inputBuffer, 0, this->cursor-1);
                int n = utf8_find_next(inputBuffer, start, 1);
                inputBuffer.erase(start, n);
                this->cursor -= 1;
                updateTextCursor();
            } else {
                int offset = utf8_find_prev(inputBuffer, 1);
                inputBuffer.erase(inputBuffer.size()-offset, offset);
            }
            updateText();
            return true;
        });

        // cancel
        dialog->getCancelEvent()->subscribe([this, eventID1]() {
            SDL_StopTextInput();
            event->unsubscribe(eventID1);
        });

        // submit
        dialog->getSubmitEvent()->subscribe([this, eventID1, cb]() {
            SDL_StopTextInput();
            event->unsubscribe(eventID1);
            cb(this->inputBuffer);
            return true;
        });

        SDL_StartTextInput();
        dialog->open();
    }

    bool SDLImeManager::openForText(std::function<void(std::string)> f, std::string headerText,
        std::string subText, int maxStringLength, std::string initialText,
        int kbdDisableBitmask)
    {
#ifdef __PSV__
        vita_ime_type = 0;
#endif
        this->openInputDialog([f](const std::string& text)
            { f(text); },
            headerText, subText, maxStringLength, initialText);
        return true;
    }

    bool SDLImeManager::openForNumber(std::function<void(long)> f, std::string headerText,
        std::string subText, int maxStringLength, std::string initialText,
        std::string leftButton, std::string rightButton,
        int kbdDisableBitmask)
    {
#ifdef __PSV__
        vita_ime_type = 2;
#endif
        this->openInputDialog([f](const std::string& text) {
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
        },headerText, subText, maxStringLength, initialText);
        return true;
    }
}