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

#pragma once

#include <borealis/core/application.hpp>
#include <borealis/core/bind.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/cells/cell_detail.hpp>
#include <borealis/views/recycler.hpp>
#include <borealis/views/slider.hpp>

namespace brls
{

class SliderCell : public DetailCell
{
  public:
    SliderCell();

    void init(const std::string& title, float init, const std::function<void(float)>& callback);

    Event<float>* getEvent();

    static View* create();

    View* getDefaultFocus() override;

    Slider* slider;
};

} // namespace brls
