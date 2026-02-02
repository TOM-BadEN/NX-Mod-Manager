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

#pragma once

#include <borealis.hpp>

class TransformBox: public brls::Image {
  public:
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    static brls::View* create();

    void setRotate(float deg);

    void setSkewX(float deg);

    void setSkewY(float deg);

    void setScaleX(float size);

    void setScaleY(float size);

    void setFontScaleX(float size);

    void setFontScaleY(float size);

  private:
    float skew_x{}, skew_y{}, rotate{}, scale_x{1}, scale_y{1};
    float font_scale_x{1}, font_scale_y{1};
};

class TransformTab : public brls::Box
{
  public:
    TransformTab();

    static brls::View* create();

  private:
    BRLS_BIND(TransformBox, box, "box");
    BRLS_BIND(brls::SliderCell, transX, "transX");
    BRLS_BIND(brls::SliderCell, transY, "transY");
    BRLS_BIND(brls::SliderCell, scaleX, "scaleX");
    BRLS_BIND(brls::SliderCell, scaleY, "scaleY");
    BRLS_BIND(brls::SliderCell, skewX, "skewX");
    BRLS_BIND(brls::SliderCell, skewY, "skewY");
    BRLS_BIND(brls::SliderCell, rotate, "rotate");
    BRLS_BIND(brls::SliderCell, boxWidth, "width");
    BRLS_BIND(brls::SliderCell, boxHeight, "height");
    BRLS_BIND(brls::SliderCell, fontScaleX, "fontScaleX");
    BRLS_BIND(brls::SliderCell, fontScaleY, "fontScaleY");
    BRLS_BIND(brls::Button, reset, "reset");
    BRLS_BIND(brls::Button, play, "play");

    static void registerCell(brls::SliderCell* cell, float init, const std::string& title, const std::function<float(float)>& cb);

    brls::Animatable aniX, aniY, skew, skew2;
};
