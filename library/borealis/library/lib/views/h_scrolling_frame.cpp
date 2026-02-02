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
#include <borealis/views/h_scrolling_frame.hpp>

namespace brls
{

#define SCROLLING_INDICATOR_HEIGHT 4

HScrollingFrame::HScrollingFrame()
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
            float newScroll = this->contentOffsetX - state.delta.x;
            startScrolling(false, newScroll);
            return;
        }

        static float startX;
        if (state.state == GestureState::START)
        {
            Application::giveFocus(this);
            startX = this->contentOffsetX;
        }

        float newScroll = startX - (state.position.x - state.startPosition.x);

        // Start animation
        if (state.state != GestureState::END)
            startScrolling(false, newScroll);
        else
        {
            float time   = state.acceleration.time.x * 1000.0f;
            float newPos = this->contentOffsetX + state.acceleration.distance.x;

            newScroll = newPos;

            if (newScroll == this->contentOffsetX || time < 100)
                return;

            animateScrolling(newScroll, time);
        }
    },
        PanAxis::HORIZONTAL));

    // Stop scrolling on tap
    addGestureRecognizer(new TapGestureRecognizer([this](brls::TapGestureStatus status, Sound* soundToPlay) {
        if (status.state == GestureState::UNSURE)
            this->contentOffsetX.stop();
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

void HScrollingFrame::setupScrollingIndicator()
{
    Theme theme        = Application::getTheme();
    scrollingIndicator = new Rectangle(theme["brls/text"]);
    scrollingIndicator->setSize(Size(0, SCROLLING_INDICATOR_HEIGHT));
    scrollingIndicator->setCornerRadius(SCROLLING_INDICATOR_HEIGHT / 2);
    scrollingIndicator->detach();
    Box::addView(scrollingIndicator);
}

void HScrollingFrame::updateScrollingIndicatior()
{
    float contentWidth = getContentWidth();
    float viewWidth    = getWidth();

    if (contentWidth <= viewWidth || !showScrollingIndicator)
    {
        scrollingIndicator->setAlpha(0);
        return;
    }

    scrollingIndicator->setAlpha(contentWidth <= viewWidth ? 0 : 0.3f);
    scrollingIndicator->setHeight(viewWidth / contentWidth * viewWidth);

    float scrollViewOffset = getContentOffsetX() / contentWidth * getWidth();
    scrollingIndicator->setDetachedPosition(scrollViewOffset, getWidth() - 14 - SCROLLING_INDICATOR_HEIGHT);
}

void HScrollingFrame::draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx)
{
    updateScrollingIndicatior();
    naturalScrollingBehaviour();

    // Update scrolling - try until it works
    if (this->updateScrollingOnNextFrame && this->updateScrolling(false))
        this->updateScrollingOnNextFrame = false;

    // Enable scissoring
    nvgSave(vg);
    float scrollingLeft    = this->getScrollingAreaLeftBoundary();
    float scrollingWidth = this->getScrollingAreaWidth();
    nvgIntersectScissor(vg, scrollingLeft, y, scrollingWidth, this->getHeight());

    // Draw children
    Box::draw(vg, x, y, width, height, style, ctx);

    //Disable scissoring
    nvgRestore(vg);
}

void HScrollingFrame::naturalScrollingBehaviour()
{
    if (behavior != ScrollingBehavior::NATURAL)
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
            View* leftMostView = findLeftMostFocusableView();

            if (leftMostView && leftMostView != currentFocus)
            {
                Application::giveFocus(leftMostView);
                Application::getAudioPlayer()->play(Sound::SOUND_FOCUS_CHANGE);
            }
        }
    }

    if (!naturalScrollingCanScroll)
        return;

    if (focused || childFocused)
    {
        auto& state      = Application::getControllerState();
        float rightLimit = this->getContentWidth() - this->getScrollingAreaWidth();

        // Sets true on border hit to play sound only once
        static bool repeat = false;

        // Do nothing if both up and down buttons pressed simultaneously
        if (state.buttons[BUTTON_NAV_RIGHT] && state.buttons[BUTTON_NAV_LEFT])
            return;

        if (state.buttons[BUTTON_NAV_RIGHT])
        {
            naturalScrollingButtonProcessing(FocusDirection::RIGHT, &repeat);
        }

        if (state.buttons[BUTTON_NAV_LEFT])
        {
            naturalScrollingButtonProcessing(FocusDirection::LEFT, &repeat);
        }

        // If there is focus inside scroll, and navigation buttons are not pressed
        // disable natural scrolling
        View* currentFocus = Application::getCurrentFocus();
        if (!state.buttons[BUTTON_NAV_RIGHT] && !state.buttons[BUTTON_NAV_LEFT] && (currentFocus != this))
        {
            naturalScrollingCanScroll = false;
        }

        // If navigation buttons are not pressed and content offset not above border
        // unflag repeat value to play border hit sound if needed
        if ((!state.buttons[BUTTON_NAV_RIGHT] && !state.buttons[BUTTON_NAV_LEFT]) || (getContentOffsetX() > 0.01f && getContentOffsetX() < rightLimit))
        {
            repeat = false;
        }
    }
}

View* HScrollingFrame::findLeftMostFocusableView()
{
    Rect frame       = getFrame();
    Point check      = Point(frame.getMinX(), frame.getMidY());
    View* focusCheck = contentView->hitTest(check);
    if (focusCheck)
    {
        View* focusCheckDefaultFocus = focusCheck->getDefaultFocus();
        if (focusCheckDefaultFocus)
            focusCheck = focusCheckDefaultFocus;

        while (focusCheck && !focusCheck->getFrame().inscribed(frame))
        {
            focusCheck = focusCheck->getParent()->getNextFocus(FocusDirection::RIGHT, focusCheck);
        }

        return focusCheck;
    }

    return nullptr;
}

void HScrollingFrame::naturalScrollingButtonProcessing(FocusDirection focusDirection, bool* repeat)
{
    float rightLimit = this->getContentWidth() - this->getScrollingAreaWidth();
    float newOffset   = getContentOffsetX();
    bool isBorder     = false;
    switch (focusDirection)
    {
        case FocusDirection::LEFT:
            isBorder = getContentOffsetX() <= 0;
            newOffset -= (1000.0f / Application::getFPS());
            break;
        case FocusDirection::RIGHT:
            isBorder = getContentOffsetX() >= rightLimit;
            newOffset += (1000.0f / Application::getFPS());
            break;
        default:
            break;
    }

    setContentOffsetX(newOffset, false);
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

void HScrollingFrame::addView(View* view)
{
    this->setContentView(view);
}

void HScrollingFrame::removeView(View* view, bool free)
{
    this->setContentView(nullptr);
}

void HScrollingFrame::setContentView(View* view)
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
    view->setHeight(this->getHeight());

    Box::addView(view); // will invalidate the scrolling box, hence calling onLayout and invalidating the contentView
}

void HScrollingFrame::onLayout()
{
    if (this->contentView)
    {
        this->contentView->setHeight(this->getHeight());
        this->contentView->invalidate();
    }
}

float HScrollingFrame::getScrollingAreaLeftBoundary()
{
    return this->getX();
}

float HScrollingFrame::getScrollingAreaWidth()
{
    return this->getWidth();
}

void HScrollingFrame::willAppear(bool resetState)
{
    this->prebakeScrolling();

    // First scroll all the way to the top
    // then wait for the first frame to scroll
    // to the selected view if needed (only known then)
    if (resetState)
    {
        this->updateScrollingOnNextFrame = true; // focus may have changed since
    }

    Box::willAppear(resetState);
}

void HScrollingFrame::prebakeScrolling()
{
    // Prebaked values for scrolling
    float x      = this->getScrollingAreaLeftBoundary();
    float width = this->getScrollingAreaWidth();

    this->middleX = x + width / 2;
    this->rightX = x + width;
}

void HScrollingFrame::startScrolling(bool animated, float newScroll)
{
    if (newScroll == this->contentOffsetX)
        return;

    if (animated)
    {
        Style style = Application::getStyle();
        animateScrolling(newScroll, style["brls/animations/highlight"]);
    }
    else
    {
        this->contentOffsetX.stop();
        this->contentOffsetX = newScroll;
        this->scrollAnimationTick();
        this->invalidate();
    }
}

void HScrollingFrame::animateScrolling(float newScroll, float time)
{
    this->contentOffsetX.stop();

    this->contentOffsetX.reset();

    this->contentOffsetX.addStep(newScroll, time, EasingFunction::quadraticOut);

    this->contentOffsetX.setTickCallback([this] {
        this->scrollAnimationTick();
    });

    this->contentOffsetX.start();

    this->invalidate();
}

void HScrollingFrame::setScrollingBehavior(ScrollingBehavior behavior)
{
    this->behavior = behavior;
}

float HScrollingFrame::getContentWidth()
{
    if (!this->contentView)
        return 0;

    return this->contentView->getWidth();
}

void HScrollingFrame::setContentOffsetX(float value, bool animated)
{
    startScrolling(animated, value);
}

void HScrollingFrame::scrollAnimationTick()
{
    if (this->contentView)
    {
        float contentWidth = this->getContentWidth();
        float rightLimit   = contentWidth - this->getScrollingAreaWidth();

        if (this->contentOffsetX < 0)
            this->contentOffsetX = 0;

        if (this->contentOffsetX > rightLimit)
            this->contentOffsetX = rightLimit;

        if (contentWidth <= getWidth())
            this->contentOffsetX = 0;

        this->contentView->setTranslationX(-this->contentOffsetX);
    }
}

View* HScrollingFrame::getNextFocus(FocusDirection direction, View* currentView)
{
    // To prevent sound click on empty scroll view
    float rightLimit    = this->getContentWidth() - this->getScrollingAreaWidth();
    float contentOffsetX = this->getContentOffsetX();
    if (direction == FocusDirection::RIGHT && contentOffsetX < (rightLimit - 0.01f))
        return this;

    if (direction == FocusDirection::LEFT && this->getContentOffsetX() > 0.01f)
        return this;

    return Box::getNextFocus(direction, currentView);
}

View* HScrollingFrame::getDefaultFocus()
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

    if (focus = findLeftMostFocusableView(); focus && focus != this)
        return focus;

    return Box::getDefaultFocus();
}

void HScrollingFrame::onFocusGained()
{
    Box::onFocusGained();
    naturalScrollingCanScroll = true;
}

void HScrollingFrame::onChildFocusGained(View* directChild, View* focusedView)
{
    Box::onChildFocusGained(directChild, focusedView);

    this->childFocused = true;

    // Start scrolling
    if (Application::getInputType() == InputType::GAMEPAD && behavior == ScrollingBehavior::CENTERED)
        this->updateScrolling(true);
}

void HScrollingFrame::onChildFocusLost(View* directChild, View* focusedView)
{
    Box::onChildFocusLost(directChild, focusedView);

    this->childFocused = false;
}

View* HScrollingFrame::getParentNavigationDecision(View* from, View* newFocus, FocusDirection direction)
{
    if (behavior == ScrollingBehavior::CENTERED)
        return Box::getParentNavigationDecision(from, newFocus, direction);

    View* currentFocus = Application::getCurrentFocus();
    if (!newFocus)
    {
        if (direction == FocusDirection::UP || direction == FocusDirection::DOWN)
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

bool HScrollingFrame::updateScrolling(bool animated)
{
    if (!this->contentView || !this->childFocused)
        return false;

    View* focusedView = getDefaultFocus();
    float localX      = focusedView->getLocalX();
    View* parent      = focusedView->getParent();

    while (parent && dynamic_cast<HScrollingFrame*>(parent->getParent()) == nullptr)
    {
        localX += parent->getLocalX();
        parent = parent->getParent();
    }

    int currentSelectionMiddleOnScreen = localX + focusedView->getWidth() / 2;
    float newScroll                    = currentSelectionMiddleOnScreen - this->getWidth() / 2;

    float contentWidth = this->getContentWidth();
    float rightLimit   = contentWidth - this->getScrollingAreaWidth();

    if (newScroll < 0)
        newScroll = 0;

    if (newScroll > rightLimit)
        newScroll = rightLimit;

    if (contentWidth <= getWidth())
        newScroll = 0;

    //Start animation
    this->startScrolling(animated, newScroll);

    return true;
}

Rect HScrollingFrame::getVisibleFrame()
{
    Rect frame = getLocalFrame();
    frame.origin.x += this->contentOffsetX;
    return frame;
}

enum Sound HScrollingFrame::getFocusSound()
{
    if (!contentView->getDefaultFocus())
    {
        return Box::getFocusSound();
    }
    return Sound::SOUND_NONE;
}

#define NO_PADDING fatal("Padding is not supported by brls:HScrollingFrame, please set padding on the content view instead");

void HScrollingFrame::setPadding(float top, float right, float bottom, float left)
{
    NO_PADDING
}

void HScrollingFrame::setPaddingTop(float top)
{
    NO_PADDING
}

void HScrollingFrame::setPaddingRight(float right)
{
    NO_PADDING
}

void HScrollingFrame::setPaddingBottom(float bottom)
{
    NO_PADDING
}

void HScrollingFrame::setPaddingLeft(float left) {
    NO_PADDING
}

View* HScrollingFrame::create()
{
    return new HScrollingFrame();
}

HScrollingFrame::~HScrollingFrame()
{
    Application::getGlobalInputTypeChangeEvent()->unsubscribe(inputTypeSubscription);
}

} // namespace brls
