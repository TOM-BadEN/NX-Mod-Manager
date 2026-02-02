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

#include "tab/transform_tab.hpp"

#define POINTER_SIZE 20
#define BOX_SIZE 100
#define CONTAINER_SIZE 400
#define ANIMATION 4000

TransformTab::TransformTab()
{
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/transform.xml");

    play->registerClickAction([this](...)
        {
            aniX.stop();
            aniY.stop();
            skew.stop();
            skew2.stop();

            aniX.reset(0);
            aniX.addStep(1.0, ANIMATION, brls::EasingFunction::exponentialOut);
            aniX.addStep(0, ANIMATION, brls::EasingFunction::cubicIn);
            aniX.addStep(0.5, ANIMATION, brls::EasingFunction::exponentialOut);
            aniX.addStep(0, ANIMATION, brls::EasingFunction::cubicIn);
            aniX.setTickCallback([this](){transX->slider->setProgress(aniX);});

            aniY.reset(0);
            aniY.addStep(1.0, ANIMATION, brls::EasingFunction::bounceOut);
            aniY.addStep(0.0, ANIMATION, brls::EasingFunction::cubicIn);
            aniY.addStep(1.0, ANIMATION, brls::EasingFunction::bounceOut);
            aniY.addStep(0.0, ANIMATION, brls::EasingFunction::cubicIn);
            aniY.setTickCallback([this](){transY->slider->setProgress(aniY);});

            skew.reset(0);
            skew.addStep(1, ANIMATION, brls::EasingFunction::bounceOut);
            skew.addStep(0, ANIMATION, brls::EasingFunction::cubicIn);
            skew.setTickCallback([this](){
                    skewY->slider->setProgress(skew);
                    scaleX->slider->setProgress(1 - skew);
                });

            skew2.reset(0);
            skew2.addStep(0, ANIMATION * 2);
            skew2.addStep(1, ANIMATION, brls::EasingFunction::bounceOut);
            skew2.addStep(0, ANIMATION, brls::EasingFunction::cubicIn);
            skew2.setTickCallback([this](){
                    skewX->slider->setProgress(skew2);
                    scaleY->slider->setProgress(1 - skew2);
                });

            aniX.start();
            aniY.start();
            skew.start();
            skew2.start();

            return true; });

    reset->registerClickAction([this](...)
        {
            aniX.stop();
            aniY.stop();
            skew.stop();
            skew2.stop();

            transX->slider->setProgress(0);
            transY->slider->setProgress(0);
            scaleX->slider->setProgress(1);
            scaleY->slider->setProgress(1);
            skewX->slider->setProgress(0);
            skewY->slider->setProgress(0);
            rotate->slider->setProgress(0);
            boxWidth->slider->setProgress(1);
            boxHeight->slider->setProgress(1);
            fontScaleX->slider->setProgress(1);
            fontScaleY->slider->setProgress(1);
            return true; });

    registerCell(transX, 0, "transX", [this](float value)
        { box->setTranslationX((CONTAINER_SIZE - BOX_SIZE) * value); return value; });

    registerCell(transY, 0, "transY", [this](float value)
        { box->setTranslationY((CONTAINER_SIZE - BOX_SIZE) * value); return value; });

    registerCell(scaleX, 1, "scaleX", [this](float value)
        { value = value * 2 - 1;
        box->setScaleX(value); return value; });

    registerCell(scaleY, 1, "scaleY", [this](float value)
        { value = value * 2 - 1;
        box->setScaleY(value); return value; });

    registerCell(skewX, 0, "skewX", [this](float value)
        { box->setSkewX(value * NVG_PI); return value; });

    registerCell(skewY, 0, "skewY", [this](float value)
        { box->setSkewY(value * NVG_PI); return value; });

    registerCell(rotate, 0, "rotate", [this](float value)
        { box->setRotate(value * NVG_PI * 2); return value; });

    registerCell(boxWidth, 1, "width", [this](float value)
        { box->setWidth(value * BOX_SIZE); return value; });

    registerCell(boxHeight, 1, "height", [this](float value)
        { box->setHeight(value * BOX_SIZE); return value; });

    registerCell(fontScaleX, 1, "fontSX", [this](float value)
        { value = value * 2 - 1;
        box->setFontScaleX(value); return value; });

    registerCell(fontScaleY, 1, "fontSY", [this](float value)
        { value = value * 2 - 1;
        box->setFontScaleY(value); return value; });
}

void TransformTab::registerCell(brls::SliderCell* cell, float init, const std::string& title, const std::function<float(float)>& cb)
{
    cell->setDetailText(fmt::format("{:.2f}", cb(init)));
    cell->slider->setPointerSize(POINTER_SIZE);
    cell->init(title, init, [cb, cell](float value)
        { cell->setDetailText(fmt::format("{:.2f}", cb(value))); });
}

brls::View* TransformTab::create()
{
    return new TransformTab();
}

void TransformBox::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx)
{
    float centerX = width * fabs(scale_x) / 2, centerY = height * fabs(scale_y) / 2;
    nvgSave(vg);
    nvgTranslate(vg, x + centerX, y + centerY);
    nvgRotate(vg, rotate);
    nvgSkewX(vg, skew_x);
    nvgSkewY(vg, skew_y);
    nvgScale(vg, scale_x, scale_y);
    brls::Image::draw(vg, -centerX, -centerY, width, height, style, ctx);
    nvgTranslate(vg, -centerX, -centerY);
    nvgScale(vg, font_scale_x, font_scale_y);
    nvgText(vg, 4, 18, "demo", nullptr);
    nvgRestore(vg);
}

void TransformBox::setRotate(float deg)
{
    rotate = deg;
}

void TransformBox::setSkewX(float deg)
{
    skew_x = deg;
}

void TransformBox::setSkewY(float deg)
{
    skew_y = deg;
}

void TransformBox::setScaleX(float value)
{
    scale_x = value;
}

void TransformBox::setScaleY(float value)
{
    scale_y = value;
}

void TransformBox::setFontScaleX(float size)
{
    font_scale_x = size;
}

void TransformBox::setFontScaleY(float size)
{
    font_scale_y = size;
}

brls::View* TransformBox::create()
{
    return new TransformBox();
}