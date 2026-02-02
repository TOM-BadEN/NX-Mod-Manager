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

#include "borealis/views/cells/cell_selector.hpp"

#include "borealis/views/dropdown.hpp"

namespace brls
{

SelectorCell::SelectorCell()
{
    detail->setTextColor(Application::getTheme()["brls/list/listItem_value_color"]);

    this->registerClickAction([this](View* view) {
        Dropdown* dropdown = new Dropdown(
            this->title->getFullText(), data, [this](int selected) {
                this->setSelection(selected, false);
            }, selection, this->dismissCb);
        Application::pushActivity(new Activity(dropdown));
        return true;
    });
}

void SelectorCell::init(std::string title, std::vector<std::string> data, int selected, Event<int>::Callback callback, Event<int>::Callback dismissCb)
{
    this->title->setText(title);
    this->data      = data;
    this->selection = selected;
    this->dismissCb = dismissCb;
    this->event.subscribe(callback);
    updateUI();
}

void SelectorCell::setSelection(int selection, bool silent)
{
    this->selection = selection;
    if (!silent)
        this->event.fire(selection);
    updateUI();
}

void SelectorCell::updateUI()
{
    auto text = data[selection];
    this->detail->setText(text);
}

View* SelectorCell::create()
{
    return new SelectorCell();
}

} // namespace brls
