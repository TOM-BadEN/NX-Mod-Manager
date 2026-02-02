/*
    Copyright 2019-2021 natinusala
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

#include <borealis/core/application.hpp>
#include <borealis/views/progress_spinner.hpp>

namespace brls
{

ProgressSpinner::ProgressSpinner(ProgressSpinnerSize size)
    : size(size)
{
    BRLS_REGISTER_ENUM_XML_ATTRIBUTE("size", ProgressSpinnerSize, this->setSize,
        {
            { "normal", ProgressSpinnerSize::NORMAL },
            { "large", ProgressSpinnerSize::LARGE },
        });
}

void ProgressSpinner::restartAnimation()
{
    Style style = Application::getStyle();

    this->animationValue.reset(0);
    this->animationValue.stop();
    this->animationValue.setEndCallback([this](bool done) {
        if (done)
            this->restartAnimation();
    });
    float animationLength = size == NORMAL ? 8.0f : 12.0f;
    this->animationValue.addStep(animationLength, style["brls/spinner/animation_duration"], EasingFunction::linear);
    this->animationValue.start();
}

void ProgressSpinner::animate(bool animate)
{
    if (animate)
    {
        this->animationValue.start();
    }
    else
    {
        this->animationValue.stop();
    }
}

void ProgressSpinner::draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx)
{
    Theme theme       = Application::getTheme();
    NVGcolor barColor = a(theme["brls/spinner/bar_color"]);

    // Each bar of the spinner
    switch (size)
    {
        case NORMAL:
            for (int i = 0 + animationValue; i < 8 + animationValue; i++)
            {
                barColor.a = fmax((i - animationValue) / 8.0f, theme["brls/spinner/bar_color"].a) * this->getAlpha();
                nvgSave(vg);
                nvgTranslate(vg, x + width / 2, y + height / 2);
                nvgRotate(vg, nvgDegToRad(i * 45)); // Internal angle of octagon
                nvgBeginPath(vg);
                nvgMoveTo(vg, height * style["brls/spinner/center_gap_multiplier"], 0);
                nvgLineTo(vg, height / 2 - height * style["brls/spinner/center_gap_multiplier"], 0);
                nvgStrokeColor(vg, barColor);
                nvgStrokeWidth(vg, height * style["brls/spinner/bar_width_multiplier"]);
                nvgLineCap(vg, NVG_SQUARE);
                nvgStroke(vg);
                nvgRestore(vg);
            }
            break;
        case LARGE:
            for (int i = 0 + animationValue; i < 12 + animationValue; i++)
            {
                barColor.a = fmax((i - animationValue) / 12.0f, theme["brls/spinner/bar_color"].a) * this->getAlpha();
                nvgSave(vg);
                nvgTranslate(vg, x + width / 2, y + height / 2);
                nvgRotate(vg, nvgDegToRad(i * 30)); // Internal angle of octagon
                nvgBeginPath(vg);
                nvgMoveTo(vg, height * style["brls/spinner/center_gap_multiplier_large"], 0);
                nvgLineTo(vg, height / 2 - height * style["brls/spinner/center_gap_multiplier_large"], 0);
                nvgStrokeColor(vg, barColor);
                nvgStrokeWidth(vg, height * style["brls/spinner/bar_width_multiplier_large"]);
                nvgLineCap(vg, NVG_SQUARE);
                nvgStroke(vg);
                nvgRestore(vg);
            }
            break;
    }
}

void ProgressSpinner::willAppear(bool resetState)
{
    this->restartAnimation();
}

void ProgressSpinner::willDisappear(bool resetState)
{
    this->animationValue.stop();
}

brls::View* ProgressSpinner::create()
{
    return new ProgressSpinner();
}

} // namespace brls
