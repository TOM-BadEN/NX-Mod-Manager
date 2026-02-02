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

#include <borealis/core/application.hpp>
#include <borealis/core/touch/pan_gesture.hpp>
#include <borealis/core/util.hpp>
#include <borealis/views/slider.hpp>

namespace brls
{

Slider::Slider()
{
    line      = new Rectangle();
    lineEmpty = new Rectangle();
    pointer   = new Rectangle();

    line->detach();
    lineEmpty->detach();
    pointer->detach();

    addView(pointer);
    addView(line);
    addView(lineEmpty);

    setHeight(40);

    line->setHeight(7);
    line->setCornerRadius(3.5f);

    lineEmpty->setHeight(7);
    lineEmpty->setCornerRadius(3.5f);

    pointer->setHeight(38);
    pointer->setWidth(38);
    pointer->setCornerRadius(19);
    pointer->setHighlightCornerRadius(21);
    pointer->setBorderThickness(4);
    pointer->setShadowType(ShadowType::GENERIC);
    pointer->setShadowVisibility(true);
    pointer->setFocusable(true);

    Theme theme = Application::getTheme();
    pointer->setColor(theme["brls/slider/pointer_color"]);
    pointer->setBorderColor(theme["brls/slider/pointer_border_color"]);

    line->setColor(theme["brls/slider/line_filled"]);
    lineEmpty->setColor(theme["brls/slider/line_empty"]);

    pointer->registerAction(
        "Right Click Blocker", BUTTON_NAV_RIGHT, [](View* view)
        { return true; },
        true, true, SOUND_NONE);

    pointer->registerAction(
        "Right Click Blocker", BUTTON_NAV_LEFT, [](View* view)
        { return true; },
        true, true, SOUND_NONE);

    pointer->registerAction(
        "A Button Click Blocker", BUTTON_A, [](View* view)
        { return true; },
        true, false, SOUND_NONE);

    pointer->addGestureRecognizer(new PanGestureRecognizer([this](PanGestureStatus status, Sound* soundToPlay)
        {
        Application::giveFocus(pointer);

        static float lastProgress = progress;

        if (status.state == GestureState::UNSURE)
        {
            *soundToPlay = SOUND_FOCUS_CHANGE;
            return;
        }

        else if (status.state == GestureState::INTERRUPTED || status.state == GestureState::FAILED)
        {
            *soundToPlay = SOUND_TOUCH_UNFOCUS;
            return;
        }

        else if (status.state == GestureState::START)
        {
            lastProgress = progress;
        }

        float paddingWidth = getWidth() - pointer->getWidth();
        float delta        = status.position.x - status.startPosition.x;

        setProgress(lastProgress + delta / paddingWidth);

        if (status.state == GestureState::END)
            Application::getPlatform()->getAudioPlayer()->play(SOUND_SLIDER_RELEASE); },
        PanAxis::HORIZONTAL));

    progress = 0.33f;
}

void Slider::onLayout()
{
    Box::onLayout();
    updateUI();
}

View* Slider::getDefaultFocus()
{
    return pointer;
}

void Slider::draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx)
{
    buttonsProcessing();
    Box::draw(vg, x, y, width, height, style, ctx);
}

void Slider::buttonsProcessing()
{
    if (pointer->isFocused())
    {
        auto& state        = Application::getControllerState();
        static bool repeat = false;

        if (state.buttons[BUTTON_NAV_RIGHT] && state.buttons[BUTTON_NAV_LEFT])
            return;

        if (state.buttons[BUTTON_NAV_RIGHT])
        {
            setProgress(progress += step / Application::getFPS());
            if (progress >= 1 && !repeat)
            {
                repeat = true;
                pointer->shakeHighlight(FocusDirection::RIGHT);
                Application::getAudioPlayer()->play(SOUND_FOCUS_ERROR);
            }
        }

        if (state.buttons[BUTTON_NAV_LEFT])
        {
            setProgress(progress -= step / Application::getFPS());
            if (progress <= 0 && !repeat)
            {
                repeat = true;
                pointer->shakeHighlight(FocusDirection::LEFT);
                Application::getAudioPlayer()->play(SOUND_FOCUS_ERROR);
            }
        }

        if ((!state.buttons[BUTTON_NAV_RIGHT] && !state.buttons[BUTTON_NAV_LEFT]) || (progress > 0.01f && progress < 0.99f))
        {
            repeat = false;
        }
    }
}

void Slider::setProgress(float progress)
{
    static int lastProgressTicker = this->progress * 10;

    this->progress = progress;

    if (this->progress < 0)
        this->progress = 0;

    if (this->progress > 1)
        this->progress = 1;

    if (lastProgressTicker != (int)(this->progress * 10))
    {
        lastProgressTicker = this->progress * 10;
        Application::getAudioPlayer()->play(SOUND_SLIDER_TICK);
    }

    progressEvent.fire(this->progress);
    updateUI();
}

void Slider::updateUI()
{
    float paddingWidth   = getWidth() - pointer->getWidth();
    float lineStart      = pointer->getWidth() / 2;
    float lineStartWidth = paddingWidth * progress;
    float lineEnd        = paddingWidth * progress + pointer->getWidth() / 2;
    float lineEndWidth   = paddingWidth * (1 - progress);
    float lineYPos       = getHeight() / 2 - line->getHeight() / 2;

    line->setDetachedPosition(lineStart, lineYPos);
    line->setWidth(lineStartWidth);

    lineEmpty->setDetachedPosition(round(lineEnd), lineYPos);
    lineEmpty->setWidth(lineEndWidth);

    pointer->setDetachedPosition(lineEnd - pointer->getWidth() / 2, getHeight() / 2 - pointer->getHeight() / 2);
}

float Slider::getProgress()
{
    return progress;
}

Event<float>* Slider::getProgressEvent()
{
    return &progressEvent;
}

void Slider::setStep(float step)
{
    this->step = step;
}

void Slider::setPointerSize(float size)
{
    this->pointer->setWidth(size);
    this->pointer->setHeight(size);
    this->updateUI();
}

View* Slider::create()
{
    return new Slider();
}

} // namespace brls
