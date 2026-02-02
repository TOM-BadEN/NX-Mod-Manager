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

#pragma once

#include <borealis/core/view.hpp>

namespace brls
{

enum ProgressSpinnerSize
{
    NORMAL,
    LARGE
};

// A progress spinner
class ProgressSpinner : public View
{
  public:
    ProgressSpinner(ProgressSpinnerSize size = ProgressSpinnerSize::NORMAL);

    void draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx) override;
    void willAppear(bool resetState = false) override;
    void willDisappear(bool resetState = false) override;

    void animate(bool animate);

    static brls::View* create();

  private:
    Animatable animationValue = 0.0f;
    ProgressSpinnerSize size;
    void setSize(ProgressSpinnerSize size)
    {
        this->size = size;
    }

    void restartAnimation();
};

} // namespace brls
