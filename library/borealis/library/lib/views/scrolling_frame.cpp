/*
    Copyright 2020-2021 natinusala
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
#include <borealis/core/touch/scroll_gesture.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/views/scrolling_frame.hpp>

namespace brls
{

#define SCROLLING_INDICATOR_WIDTH 4

ScrollingFrame::ScrollingFrame()
{
    BRLS_REGISTER_ENUM_XML_ATTRIBUTE(
        "scrollingBehavior", ScrollingBehavior, this->setScrollingBehavior,
        {
            { "natural", ScrollingBehavior::NATURAL },
            { "centered", ScrollingBehavior::CENTERED },
        });

    setupScrollingIndicator();

    this->setFocusable(true);
    this->setMaximumAllowedXMLElements(1);

    addGestureRecognizer(new ScrollGestureRecognizer([this](PanGestureStatus state, Sound* soundToPlay) {
        if (state.state == GestureState::FAILED || state.state == GestureState::UNSURE || state.state == GestureState::INTERRUPTED)
            return;

        if (state.deltaOnly)
        {
            float newScroll = this->contentOffsetY - state.delta.y;
            startScrolling(false, newScroll);
            return;
        }

        static float startY;
        if (state.state == GestureState::START)
        {
            Application::giveFocus(this);
            startY = this->contentOffsetY;
        }

        float newScroll = startY - (state.position.y - state.startPosition.y);

        // Start animation
        if (state.state != GestureState::END)
            startScrolling(false, newScroll);
        else
        {
            float time   = state.acceleration.time.y * 1000.0f;
            float newPos = this->contentOffsetY + state.acceleration.distance.y;

            newScroll = newPos;

            if (newScroll == this->contentOffsetY || time < 100)
                return;

            animateScrolling(newScroll, time);
        }
    },
        PanAxis::VERTICAL));

    // Stop scrolling on tap
    addGestureRecognizer(new TapGestureRecognizer([this](brls::TapGestureStatus status, Sound* soundToPlay) {
        if (status.state == GestureState::UNSURE)
            this->contentOffsetY.stop();
    }));

    inputTypeSubscription = Application::getGlobalInputTypeChangeEvent()->subscribe([this](InputType type) {
        if (!focused && !childFocused)
            return;

        if (behavior == ScrollingBehavior::NATURAL && type == InputType::GAMEPAD)
        {
            Application::giveFocus(getDefaultFocus());
            naturalScrollingCanScroll = false;
        }
    });

    setHideHighlightBackground(true);
    setHideHighlightBorder(true);
}

void ScrollingFrame::setupScrollingIndicator()
{
    Theme theme        = Application::getTheme();
    scrollingIndicator = new Rectangle(theme["brls/text"]);
    scrollingIndicator->setSize(Size(SCROLLING_INDICATOR_WIDTH, 0));
    scrollingIndicator->setCornerRadius(SCROLLING_INDICATOR_WIDTH / 2);
    scrollingIndicator->detach();
    Box::addView(scrollingIndicator);
}

void ScrollingFrame::updateScrollingIndicatior()
{
    float contentHeight = getContentHeight();
    float viewHeight    = getHeight();

    if (contentHeight <= viewHeight || !showScrollingIndicator)
    {
        scrollingIndicator->setAlpha(0);
        return;
    }

    scrollingIndicator->setAlpha(contentHeight <= viewHeight ? 0 : 0.3f);
    scrollingIndicator->setHeight(viewHeight / contentHeight * viewHeight);

    float scrollViewOffset = getContentOffsetY() / contentHeight * getHeight();
    scrollingIndicator->setDetachedPosition(getWidth() - 14 - SCROLLING_INDICATOR_WIDTH, scrollViewOffset);
}

void ScrollingFrame::draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx)
{
    updateScrollingIndicatior();
    naturalScrollingBehaviour();

    // Update scrolling - try until it works
    if (this->updateScrollingOnNextFrame && this->updateScrolling(false))
        this->updateScrollingOnNextFrame = false;

    // Enable scissoring
    nvgSave(vg);
    float scrollingTop    = this->getScrollingAreaTopBoundary();
    float scrollingHeight = this->getScrollingAreaHeight();
    nvgIntersectScissor(vg, x, scrollingTop, this->getWidth(), scrollingHeight);

    // Draw children
    Box::draw(vg, x, y, width, height, style, ctx);

    //Disable scissoring
    nvgRestore(vg);
}

void ScrollingFrame::naturalScrollingBehaviour()
{
    if (behavior != ScrollingBehavior::NATURAL || Application::getInputType() == InputType::TOUCH)
        return;

    if (focused || childFocused)
    {
        // If current focus view is outside scrolling bounds,
        // change focus to this.
        View* currentFocus = Application::getCurrentFocus();
        if (!currentFocus->getFrame().inscribed(getFrame()))
        {
            Application::giveFocus(this);
        }

        // If current focus equals this (a.k. no focus inside scroll),
        // try to find the closest to the top focusable view and set it as current focus.
        if (Application::getCurrentFocus() == this && Application::getInputType() == InputType::GAMEPAD)
        {
            View* topMostView = findTopMostFocusableView();

            if (topMostView && topMostView != currentFocus)
            {
                Application::giveFocus(topMostView);
                Application::getAudioPlayer()->play(Sound::SOUND_FOCUS_CHANGE);
            }
        }
    }

    if (!naturalScrollingCanScroll)
        return;

    if (focused || childFocused)
    {
        auto& state       = Application::getControllerState();
        float bottomLimit = this->getContentHeight() - this->getScrollingAreaHeight();

        // Sets true on border hit to play sound only once
        static bool repeat = false;

        // Do nothing if both up and down buttons pressed simultaneously
        if (state.buttons[BUTTON_NAV_DOWN] && state.buttons[BUTTON_NAV_UP])
            return;

        if (state.buttons[BUTTON_NAV_DOWN])
        {
            naturalScrollingButtonProcessing(FocusDirection::DOWN, &repeat);
        }

        if (state.buttons[BUTTON_NAV_UP])
        {
            naturalScrollingButtonProcessing(FocusDirection::UP, &repeat);
        }

        // If there is focus inside scroll, and navigation buttons are not pressed
        // disable natural scrolling
        View* currentFocus = Application::getCurrentFocus();
        if (!state.buttons[BUTTON_NAV_DOWN] && !state.buttons[BUTTON_NAV_UP] && (currentFocus != this))
        {
            naturalScrollingCanScroll = false;
        }

        // If navigation buttons are not pressed and content offset not above border
        // unflag repeat value to play border hit sound if needed
        if ((!state.buttons[BUTTON_NAV_DOWN] && !state.buttons[BUTTON_NAV_UP]) || (getContentOffsetY() > 0.01f && getContentOffsetY() < bottomLimit))
        {
            repeat = false;
        }
    }
}

View* ScrollingFrame::findTopMostFocusableView()
{
    Rect frame       = getFrame();
    Point check      = Point(frame.getMidX(), frame.getMinY());
    View* focusCheck = contentView->hitTest(check);
    if (focusCheck)
    {
        View* focusCheckDefaultFocus = focusCheck->getDefaultFocus();
        if (focusCheckDefaultFocus)
            focusCheck = focusCheckDefaultFocus;

        while (focusCheck && !focusCheck->getFrame().inscribed(frame))
        {
            focusCheck = focusCheck->getParent()->getNextFocus(FocusDirection::DOWN, focusCheck);
        }

        return focusCheck;
    }

    return nullptr;
}

void ScrollingFrame::naturalScrollingButtonProcessing(FocusDirection focusDirection, bool* repeat)
{
    float bottomLimit = this->getContentHeight() - this->getScrollingAreaHeight();
    float newOffset   = getContentOffsetY();
    bool isBorder     = false;
    switch (focusDirection)
    {
        case FocusDirection::UP:
            isBorder = getContentOffsetY() <= 0;
            newOffset -= (1000.0f / Application::getFPS());
            break;
        case FocusDirection::DOWN:
            isBorder = getContentOffsetY() >= bottomLimit;
            newOffset += (1000.0f / Application::getFPS());
            break;
        default:
            break;
    }

    setContentOffsetY(newOffset, false);
    View* current = Application::getCurrentFocus();
    View* next    = current->getParent()->getNextFocus(focusDirection, current);
    if (next)
    {
        if (current != next->getDefaultFocus())
        {
            Application::giveFocus(next);
            if (next != this)
                Application::getAudioPlayer()->play(SOUND_FOCUS_CHANGE);
        }
    }
    else if (!current->getFrame().inscribed(getFrame()))
    {
        Application::giveFocus(this);
    }

    if (isBorder && !*repeat)
    {
        *repeat = true;
        Application::getCurrentFocus()->shakeHighlight(focusDirection);
        Application::getAudioPlayer()->play(SOUND_FOCUS_ERROR);
    }
}

void ScrollingFrame::addView(View* view)
{
    this->setContentView(view);
}

void ScrollingFrame::removeView(View* view, bool free)
{
    this->setContentView(nullptr);
}

void ScrollingFrame::setContentView(View* view)
{
    if (this->contentView)
    {
        Box::removeView(this->contentView); // will delete and call willDisappear
        this->contentView = nullptr;
    }

    if (!view)
        return;

    // Setup the view and add it
    this->contentView = view;

    view->detach();
    view->setCulled(false);
    view->setWidth(this->getWidth());

    Box::addView(view); // will invalidate the scrolling box, hence calling onLayout and invalidating the contentView
}

void ScrollingFrame::onLayout()
{
    if (this->contentView)
    {
        this->contentView->setWidth(this->getWidth());
        this->contentView->invalidate();
    }
}

float ScrollingFrame::getScrollingAreaTopBoundary()
{
    return this->getY();
}

float ScrollingFrame::getScrollingAreaHeight()
{
    return this->getHeight();
}

void ScrollingFrame::willAppear(bool resetState)
{
    this->prebakeScrolling();

    // First scroll all the way to the top
    // then wait for the first frame to scroll
    // to the selected view if needed (only known then)
    if (resetState && behavior == ScrollingBehavior::CENTERED)
    {
        this->updateScrollingOnNextFrame = true; // focus may have changed since
    }

    Box::willAppear(resetState);
}

void ScrollingFrame::prebakeScrolling()
{
    // Prebaked values for scrolling
    float y      = this->getScrollingAreaTopBoundary();
    float height = this->getScrollingAreaHeight();

    this->middleY = y + height / 2;
    this->bottomY = y + height;
}

void ScrollingFrame::startScrolling(bool animated, float newScroll)
{
    if (newScroll == this->contentOffsetY)
        return;

    if (animated)
    {
        Style style = Application::getStyle();
        animateScrolling(newScroll, style["brls/animations/highlight"]);
    }
    else
    {
        this->contentOffsetY.stop();
        this->contentOffsetY = newScroll;
        this->scrollAnimationTick();
        this->invalidate();
    }
}

void ScrollingFrame::animateScrolling(float newScroll, float time)
{
    this->contentOffsetY.stop();

    this->contentOffsetY.reset();

    this->contentOffsetY.addStep(newScroll, time, EasingFunction::quadraticOut);

    this->contentOffsetY.setTickCallback([this] {
        this->scrollAnimationTick();
    });

    this->contentOffsetY.start();

    this->invalidate();
}

void ScrollingFrame::setScrollingBehavior(ScrollingBehavior behavior)
{
    this->behavior = behavior;
}

float ScrollingFrame::getContentHeight()
{
    if (!this->contentView)
        return 0;

    return this->contentView->getHeight();
}

void ScrollingFrame::setContentOffsetY(float value, bool animated)
{
    startScrolling(animated, value);
}

void ScrollingFrame::scrollAnimationTick()
{
    if (this->contentView)
    {
        float contentHeight = this->getContentHeight();
        float bottomLimit   = contentHeight - this->getScrollingAreaHeight();

        if (this->contentOffsetY < 0)
            this->contentOffsetY = 0;

        if (this->contentOffsetY > bottomLimit)
            this->contentOffsetY = bottomLimit;

        if (contentHeight <= getHeight())
            this->contentOffsetY = 0;

        this->contentView->setTranslationY(-this->contentOffsetY);
    }
}

View* ScrollingFrame::getNextFocus(FocusDirection direction, View* currentView)
{
    // To prevent sound click on empty scroll view
    float bottomLimit    = this->getContentHeight() - this->getScrollingAreaHeight();
    float contentOffsetY = this->getContentOffsetY();
    if (direction == FocusDirection::DOWN && contentOffsetY < (bottomLimit - 0.01f))
        return this;

    if (direction == FocusDirection::UP && this->getContentOffsetY() > 0.01f)
        return this;

    return Box::getNextFocus(direction, currentView);
}

View* ScrollingFrame::getDefaultFocus()
{
    if (behavior == ScrollingBehavior::CENTERED)
    {
        View* focus = contentView->getDefaultFocus();
        if (focus)
            return focus;
        else
            return Box::getDefaultFocus();
    }

    View* focus = contentView->getDefaultFocus();
    if (focus && focus->getFrame().inscribed(getFrame()))
        return focus;

    if (focus = findTopMostFocusableView(); focus && focus != this)
        return focus;

    return Box::getDefaultFocus();
}

void ScrollingFrame::onFocusGained()
{
    Box::onFocusGained();
    naturalScrollingCanScroll = true;
}

void ScrollingFrame::onChildFocusGained(View* directChild, View* focusedView)
{
    Box::onChildFocusGained(directChild, focusedView);

    this->childFocused = true;

    // Start scrolling
    if (Application::getInputType() == InputType::GAMEPAD && behavior == ScrollingBehavior::CENTERED)
        this->updateScrolling(true);
}

void ScrollingFrame::onChildFocusLost(View* directChild, View* focusedView)
{
    this->childFocused = false;
}

View* ScrollingFrame::getParentNavigationDecision(View* from, View* newFocus, FocusDirection direction)
{
    if (behavior == ScrollingBehavior::CENTERED)
        return Box::getParentNavigationDecision(from, newFocus, direction);

    View* currentFocus = Application::getCurrentFocus();
    if (!newFocus)
    {
        if (direction == FocusDirection::LEFT || direction == FocusDirection::RIGHT)
            return nullptr;

        if (from == contentView)
        {
            naturalScrollingCanScroll = true;
            if (currentFocus->getFrame().inscribed(this->getFrame()))
                return currentFocus;

            return this;
        }
        return nullptr;
    }
    else
    {
        if (newFocus->getFrame().inscribed(this->getFrame()))
            return newFocus;
        else
            naturalScrollingCanScroll = true;
    }

    if (currentFocus->getFrame().inscribed(this->getFrame()))
        return currentFocus;

    return this;
}

bool ScrollingFrame::updateScrolling(bool animated)
{
    if (!this->contentView)
        return false;

    View* focusedView = getDefaultFocus();
    float localY      = focusedView ? focusedView->getLocalY() : 0.0f;
    float itemHeight  = focusedView ? focusedView->getHeight() : 0.0f;
    View* parent      = focusedView ? focusedView->getParent() : nullptr;

    while (parent && dynamic_cast<ScrollingFrame*>(parent->getParent()) == nullptr)
    {
        localY += parent->getLocalY();
        parent = parent->getParent();
    }

    int currentSelectionMiddleOnScreen = localY + itemHeight / 2;
    float newScroll                    = currentSelectionMiddleOnScreen - this->getHeight() / 2;

    float contentHeight = this->getContentHeight();
    float bottomLimit   = contentHeight - this->getScrollingAreaHeight();

    if (newScroll < 0)
        newScroll = 0;

    if (newScroll > bottomLimit)
        newScroll = bottomLimit;

    if (contentHeight <= getHeight())
        newScroll = 0;

    //Start animation
    this->startScrolling(animated, newScroll);

    return true;
}

Rect ScrollingFrame::getVisibleFrame()
{
    Rect frame = getLocalFrame();
    frame.origin.y += this->contentOffsetY;
    return frame;
}

enum Sound ScrollingFrame::getFocusSound()
{
    if (!contentView->getDefaultFocus())
    {
        return Box::getFocusSound();
    }
    return Sound::SOUND_NONE;
}

#define NO_PADDING fatal("Padding is not supported by brls:ScrollingFrame, please set padding on the content view instead");

void ScrollingFrame::setPadding(float top, float right, float bottom, float left)
{
    NO_PADDING
}

void ScrollingFrame::setPaddingTop(float top)
{
    NO_PADDING
}

void ScrollingFrame::setPaddingRight(float right)
{
    NO_PADDING
}

void ScrollingFrame::setPaddingBottom(float bottom)
{
    NO_PADDING
}

void ScrollingFrame::setPaddingLeft(float left) {
    NO_PADDING
}

View* ScrollingFrame::create()
{
    return new ScrollingFrame();
}

ScrollingFrame::~ScrollingFrame()
{
    Application::getGlobalInputTypeChangeEvent()->unsubscribe(inputTypeSubscription);
}

} // namespace brls
