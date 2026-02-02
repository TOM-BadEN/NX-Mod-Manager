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

#pragma once

#include <borealis/core/animation.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/rectangle.hpp>

namespace brls
{

enum class ScrollingBehavior
{
    // Inputs scroll the view like the scroll wheel on a web page, focus changes only when the next view to focus is fully on screen
    // To work properly, there must be at least one focusable view in the "top" area of the frame (there should not be the need to scroll to see it)
    NATURAL,

    // The focused view is always in the center, inputs always change focus and scroll immediately
    CENTERED,
};

// A vertical-only frame that can scroll if its content overflows.
// This frame can only contain one child view.
// The content view is detached from the rest of the tree
// so that its height can grow as much as possible.
class ScrollingFrame : public Box
{
  public:
    ScrollingFrame();
    ~ScrollingFrame();

    void draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx) override;
    void onFocusGained() override;
    void onChildFocusGained(View* directChild, View* focusedView) override;
    void onChildFocusLost(View* directChild, View* focusedView) override;
    void willAppear(bool resetState) override;
    void addView(View* view) override;
    void removeView(View* view, bool free = true) override;
    void onLayout() override;
    void setPadding(float top, float right, float bottom, float left) override;
    void setPaddingTop(float top) override;
    void setPaddingRight(float right) override;
    void setPaddingBottom(float bottom) override;
    void setPaddingLeft(float left) override;
    View* getParentNavigationDecision(View* from, View* newFocus, FocusDirection direction) override;
    View* getNextFocus(FocusDirection direction, View* currentView) override;
    View* getDefaultFocus() override;
    enum Sound getFocusSound() override;
    Rect getVisibleFrame();

    /**
     * Sets the content view of this scrolling box. There can only be one
     * content view per scrolling box at a time.
     */
    void setContentView(View* view);

    /**
     * Sets the scrolling behavior of this scrolling frame.
     * Default is NATURAL.
     */
    void setScrollingBehavior(ScrollingBehavior behavior);

    /**
     * The point at which the origin of the content view is offset from the origin of the scroll view.
     */
    float getContentOffsetY() const
    {
        return contentOffsetY;
    }

    /**
     * Sets the offset from the content view’s origin that corresponds to the receiver’s origin.
     */
    void setContentOffsetY(float value, bool animated);

    void setScrollingIndicatorVisible(bool visible)
    {
        showScrollingIndicator = visible;
    }

    static View* create();

  protected:
    View* contentView             = nullptr;
    Rectangle* scrollingIndicator = nullptr;

    bool updateScrollingOnNextFrame = false;
    bool childFocused               = false;
    bool showScrollingIndicator     = true;

    float middleY = 0; // y + height/2
    float bottomY = 0; // y + height

    Animatable contentOffsetY = 0.0f;

    void prebakeScrolling();
    bool updateScrolling(bool animated);
    void startScrolling(bool animated, float newScroll);
    void animateScrolling(float newScroll, float time);
    void scrollAnimationTick();

    float getScrollingAreaTopBoundary();
    float getScrollingAreaHeight();

    float getContentHeight();

    ScrollingBehavior behavior = ScrollingBehavior::NATURAL;
    bool naturalScrollingCanScroll = false;
    void naturalScrollingBehaviour();
    void naturalScrollingButtonProcessing(FocusDirection focusDirection, bool* repeat);
    View* findTopMostFocusableView();

    void setupScrollingIndicator();
    void updateScrollingIndicatior();

    Event<InputType>::Subscription inputTypeSubscription;
};

} // namespace brls
