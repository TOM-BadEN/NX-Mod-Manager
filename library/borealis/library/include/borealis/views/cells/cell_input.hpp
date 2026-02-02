/*
    Copyright 2021 XITRIX

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

#include <borealis/core/application.hpp>
#include <borealis/core/bind.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/recycler.hpp>
#include <string>

namespace brls
{

class InputCell : public DetailCell
{
  public:
    InputCell();

    void init(std::string title, std::string value, Event<std::string>::Callback callback = [](std::string text){}, std::string placeholder = "", std::string hint = "", int maxInputLength = 32, int kbdDisableBitmask = 0);

    void setValue(std::string value);
    std::string getValue()
    {
        return value;
    }

    void setPlaceholder(std::string placeholder);
    std::string getPlaceholder()
    {
        return placeholder;
    }

    void setHint(std::string hint)
    {
        this->hint = hint;
    }

    std::string getHint()
    {
        return hint;
    }

    Event<std::string>* getEvent()
    {
        return &event;
    }

    static View* create();

  private:
    std::string value;
    std::string hint;
    std::string placeholder;
    int maxInputLength;
    int kbdDisableBitmask;

    Event<std::string> event;
    void updateUI();
};

class InputNumericCell : public DetailCell
{
  public:
    InputNumericCell();

    void init(std::string title, long value, Event<long>::Callback callback, std::string hint = "", int maxInputLength = 18, int kbdDisableBitmask = 0);

    void setValue(long value);
    long getValue()
    {
        return value;
    }

    void setHint(std::string hint)
    {
        this->hint = hint;
    }

    std::string getHint()
    {
        return hint;
    }

    Event<long>* getEvent()
    {
        return &event;
    }

    static View* create();

  private:
    long value;
    std::string hint;
    int maxInputLength;
    int kbdDisableBitmask;

    Event<long> event;
    void updateUI();
};

} // namespace brls
