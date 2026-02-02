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

#pragma once

#include <borealis/core/application.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/image.hpp>
#include <borealis/views/rectangle.hpp>

namespace brls
{

class BatteryWidget : public Box
{
  public:
    BatteryWidget();

    void draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx) override;
    static View* create();

  private:
    Image* back;
    Rectangle* level;
    Platform* platform;

    void applyBackTheme(ThemeVariant theme);
    void applyLevelTheme(ThemeVariant theme);

    void updateState();
    inline static bool isBatteryCharging = false;
    inline static float batteryLevel     = 1;
};

} // namespace brls
