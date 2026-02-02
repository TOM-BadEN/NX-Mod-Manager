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

#include "borealis/views/cells/cell_slider.hpp"

#include <borealis/core/i18n.hpp>

using namespace brls::literals;

namespace brls
{

SliderCell::SliderCell()
{
    this->setFocusable(false);
    title->setGrow(0);
    detail->setWidth(50);
    slider = new Slider();
    slider->setGrow(1);
    slider->setMarginRight(16);
    slider->setFocusable(true);
    this->addView(slider, 1);
}

void SliderCell::init(const std::string& title, float init, const std::function<void(float)>& callback)
{
    this->title->setText(title);
    this->slider->setProgress(init);
    this->slider->getProgressEvent()->subscribe(callback);
}

Event<float>* SliderCell::getEvent() {
    return this->slider->getProgressEvent();
}

View* SliderCell::getDefaultFocus()
{
    return slider->getDefaultFocus();
}

View* SliderCell::create()
{
    return new SliderCell();
}

} // namespace brls
