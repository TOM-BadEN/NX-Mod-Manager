/*
    Copyright 2020 natinusala

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

#include <borealis/core/activity.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/i18n.hpp>

using namespace brls::literals;

namespace brls
{

Activity::Activity()
{
}

Activity::Activity(View* view)
    : constructorView(view)
{
}

void Activity::setContentView(View* view)
{
    if (this->contentView)
    {
        this->contentView->setParentActivity(nullptr);
        this->contentView->willDisappear();
        delete this->contentView;
    }

    this->contentView = view;
    this->contentView->setParentActivity(this);
    // willAppear is only called when the activity is pushed onto the stack

    this->resizeToFitWindow();
}

void Activity::resizeToFitWindow()
{
    if (!this->contentView)
        return;

    this->contentView->setDimensions(Application::contentWidth, Application::contentHeight);
}

View* Activity::createContentView()
{
    return constructorView;
}

float Activity::getShowAnimationDuration(TransitionAnimation animation)
{
    return contentView->getShowAnimationDuration(animation);
}

void Activity::onWindowSizeChanged()
{
    this->resizeToFitWindow();
}

bool Activity::isTranslucent()
{
    if (!this->contentView)
        return true;

    return this->contentView->isTranslucent();
}

void Activity::willAppear(bool resetState)
{
    if (this->contentView)
        this->contentView->willAppear(resetState);
}

void Activity::willDisappear(bool resetState)
{
    if (this->contentView)
        this->contentView->willDisappear(resetState);
}

void Activity::setInFadeAnimation(bool inFadeAnimation)
{
    if (this->contentView)
        this->contentView->setInFadeAnimation(inFadeAnimation);
}

void Activity::show(std::function<void(void)> cb, bool animate, float animationDuration)
{
    if (this->contentView)
        this->contentView->show(cb, animate, animationDuration);
}

void Activity::hide(std::function<void(void)> cb, bool animate, float animationDuration)
{
    if (this->contentView)
        this->contentView->hide(cb, animate, animationDuration);
}

bool Activity::isHidden()
{
    if (!this->contentView)
        return false;

    return this->contentView->isHidden();
}

ActionIdentifier Activity::registerAction(const std::string& hintText, const enum ControllerButton button, const ActionListener& actionListener, const bool hidden, const bool allowRepeating, const enum Sound sound)
{
    if (this->contentView)
        return this->contentView->registerAction(hintText, button, actionListener, hidden, allowRepeating, sound);

    return ACTION_NONE;
}

ActionIdentifier Activity::registerAction(const BrlsKeyCombination key, const ActionListener& actionListener, const bool allowRepeating)
{
    if (this->contentView)
        return this->contentView->registerAction(key, actionListener, allowRepeating);

    return ACTION_NONE;
}

void Activity::unregisterAction(ActionIdentifier identifier)
{
    if (this->contentView)
        this->contentView->unregisterAction(identifier);
}

ActionIdentifier Activity::registerExitAction(enum ControllerButton button)
{
    if (this->contentView)
        return this->contentView->registerAction("hints/exit"_i18n, button, [](View* view) { Application::quit(); return true; });

    return ACTION_NONE;
}

View* Activity::getDefaultFocus()
{
    if (!this->contentView)
        return nullptr;

    return this->contentView->getDefaultFocus();
}

void Activity::setAlpha(float alpha)
{
    if (this->contentView)
        this->contentView->setAlpha(alpha);
}

View* Activity::getContentView()
{
    return this->contentView;
}

View* Activity::getView(std::string id)
{
    if (!this->contentView)
        return nullptr;

    return this->contentView->getView(id);
}

Activity::~Activity()
{
    if (this->contentView)
    {
        this->contentView->willDisappear();

        this->contentView->freeView();
        this->contentView = nullptr;
    }
}

} // namespace brls
