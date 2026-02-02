/*
    Copyright 2020 WerWolv

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

#include <borealis/core/audio.hpp>
#include <borealis/core/input.hpp>
#include <functional>
#include <string>

namespace brls
{

class View;

typedef std::function<bool(View*)> ActionListener;

typedef int ActionIdentifier;

typedef int BrlsKeyCode;

struct BrlsKeyCombination
{
    BrlsKeyboardScancode code;
    int mod;

    explicit BrlsKeyCombination(const int key)
    {
        code = static_cast<BrlsKeyboardScancode>(key & 0xFFFF);
        mod  = static_cast<BrlsKeyboardModifiers>((key >> 16) & 0xFFFF);
    }

    BrlsKeyCombination(const BrlsKeyboardScancode code) : code(code), mod(BRLS_KBD_MODIFIER_NONE) {}

    BrlsKeyCombination(const BrlsKeyboardScancode code, const int mod) : code(code), mod(mod) {}

    operator int() const
    {
        return mod << 16 | code;
    }

    bool operator ==(const int a) const
    {
        return (mod << 16 | code) == a;
    }

    bool operator !=(const int a) const
    {
        return (mod << 16 | code) != a;
    }
};

class BrlsKeyState
{
public:
    explicit BrlsKeyState(const BrlsKeyCombination keyComb): key(keyComb) {}

    BrlsKeyCombination key;
    bool pressed{};
    Time repeatingStop{};

    [[nodiscard]] int getModifiers() const
    {
        return this->key.mod;
    }

    [[nodiscard]] BrlsKeyboardScancode getScancode() const
    {
        return this->key.code;
    }
};

#define ACTION_NONE -1

enum ActionType
{
    ACTION_GAMEPAD,
    ACTION_KEYBOARD,
    ACTION_COUNT,
};

class Action
{
public:
    Action(const int button, const ActionIdentifier identifier, const std::string& hintText,
        const bool available, const bool hidden, const bool allowRepeating, const Sound sound, const ActionListener& actionListener)
    {
        this->type           = ACTION_COUNT;
        this->button         = button;
        this->identifier     = identifier;
        this->hintText       = hintText;
        this->available      = available;
        this->hidden         = hidden;
        this->allowRepeating = allowRepeating;
        this->sound          = sound;
        this->actionListener = actionListener;
    }

    virtual ~Action() = default;

    virtual bool operator==(const ControllerButton other) const
    {
        return this->type == ACTION_GAMEPAD && this->button == other;
    }

    virtual bool operator==(const BrlsKeyCode other) const
    {
        return this->type == ACTION_KEYBOARD && this->button == other;
    }

    [[nodiscard]] ActionIdentifier getIdentifier() const
    {
        return this->identifier;
    }

    [[nodiscard]] ActionType getType() const
    {
        return this->type;
    }

    [[nodiscard]] int getButton() const
    {
        return this->button;
    }

    [[nodiscard]] std::string getHintText() const
    {
        return this->hintText;
    }

    void setHintText(const std::string& hintText)
    {
        this->hintText = hintText;
    }

    [[nodiscard]] bool isAvailable() const
    {
        return this->available;
    }

    void setAvailable(const bool isAvailable)
    {
        this->available = isAvailable;
    }

    [[nodiscard]] bool isHidden() const
    {
        return this->hidden;
    }

    [[nodiscard]] bool isAllowRepeating() const
    {
        return this->allowRepeating;
    }

    [[nodiscard]] Sound getSound() const
    {
        return this->sound;
    }

    [[nodiscard]] ActionListener getActionListener() const
    {
        return this->actionListener;
    }

protected:
    ActionType type{};
    int button{};
    ActionIdentifier identifier{};
    std::string hintText{};
    bool available{};
    bool hidden{};
    bool allowRepeating{};
    Sound sound{};
    ActionListener actionListener{};
};

class KeyboardAction final : public Action
{
public:
    KeyboardAction(const BrlsKeyCode button, const ActionIdentifier identifier, const std::string& hintText,
        const bool available, const bool hidden, const bool allowRepeating, const Sound sound, const ActionListener& actionListener):
        Action(button, identifier, hintText, available, hidden, allowRepeating, sound, actionListener)
    {
        this->type = ACTION_KEYBOARD;
    }
};

class GamepadAction final : public Action
{
public:
    GamepadAction(const ControllerButton button, const ActionIdentifier identifier, const std::string& hintText,
        const bool available, const bool hidden, const bool allowRepeating, const Sound sound, const ActionListener& actionListener):
        Action(button, identifier, hintText, available, hidden, allowRepeating, sound, actionListener)
    {
        this->type = ACTION_GAMEPAD;
    }
};

} // namespace brls