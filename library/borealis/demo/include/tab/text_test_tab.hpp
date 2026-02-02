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

class TextTestTab : public brls::Box
{
  public:
    TextTestTab();

    static brls::View* create();

  private:
    BRLS_BIND(brls::SliderCell, width, "width");
    BRLS_BIND(brls::SliderCell, height, "height");
    BRLS_BIND(brls::BooleanCell, singleLine, "singleLine");
    BRLS_BIND(brls::SelectorCell, horizontal, "horizontal");
    BRLS_BIND(brls::SelectorCell, vertical, "vertical");
    BRLS_BIND(brls::Label, label, "label");
};
