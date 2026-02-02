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

#include "tab/text_test_tab.hpp"

static void registerSliderCell(brls::SliderCell* cell, float init, const std::string& title, const std::function<int(float)>& cb)
{
    int res = cb(init);
    cell->setDetailText(res == 0 ? "auto" : fmt::format("{}", res));
    cell->slider->setPointerSize(20);
    cell->init(title, init, [cb, cell](float value)
        {
            int res = cb(value);
            cell->setDetailText(res == 0 ? "auto" : fmt::format("{}", res)); });
}

TextTestTab::TextTestTab()
{
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/text_test.xml");

    registerSliderCell(width, 1.0f, "width", [this](float value)
        {
            value *= 400;
            label->setWidth(value <= 0.0f ? brls::View::AUTO : value); return value; });

    registerSliderCell(height, 0.0f, "height", [this](float value)
        {
            value *= 400;
            singleLine->setVisibility(value <= 0.0f ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
            label->setHeight(value <= 0.0f ? brls::View::AUTO : value); return value; });

    vertical->init("verticalAlign", { "baseline", "top", "center", "bottom" }, 2, [this](int value)
        {
            label->setVerticalAlign((brls::VerticalAlign)value); return value; });

    horizontal->init("horizontalAlign", { "left", "center", "right" }, 0, [this](int value)
        {
            label->setHorizontalAlign((brls::HorizontalAlign)value); return value; });

    singleLine->init("singleLine", false, [this](bool value)
        {
            label->setSingleLine(value); return value; });
}

brls::View* TextTestTab::create()
{
    return new TextTestTab();
}