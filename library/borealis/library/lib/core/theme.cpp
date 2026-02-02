/*
    Copyright 2019 natinusala
    Copyright 2019 p-sam

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

#include <borealis/core/theme.hpp>
#include <borealis/core/util.hpp>
#include <stdexcept>

namespace brls
{

static ThemeValues lightThemeValues = {
    // Generic values
    { "brls/clear", nvgRGB(235, 235, 235) },
    { "brls/background", nvgRGB(235, 235, 235) },
    { "brls/text", nvgRGB(45, 45, 45) },
    { "brls/text_disabled", nvgRGB(140, 140, 140) },
    { "brls/backdrop", nvgRGBA(0, 0, 0, 178) },
    { "brls/click_pulse", nvgRGBA(13, 182, 213, 38) }, // same as highlight color1 with different opacity
    { "brls/accent", nvgRGB(49, 79, 235) },

    // Highlight
    { "brls/highlight/background", nvgRGB(252, 255, 248) },
    { "brls/highlight/color1", nvgRGB(13, 182, 213) },
    { "brls/highlight/color2", nvgRGB(80, 239, 217) },

    // AppletFrame
    { "brls/applet_frame/separator", nvgRGB(45, 45, 45) },

    // Sidebar
    { "brls/sidebar/background", nvgRGB(240, 240, 240) },
    { "brls/sidebar/active_item", nvgRGB(49, 79, 235) },
    { "brls/sidebar/separator", nvgRGB(208, 208, 208) },

    // Header
    { "brls/header/border", nvgRGB(207, 207, 207) },
    { "brls/header/rectangle", nvgRGB(127, 127, 127) },
    { "brls/header/subtitle", nvgRGB(140, 140, 140) },

    // Button
    { "brls/button/primary_enabled_background", nvgRGB(50, 79, 241) },
    { "brls/button/primary_disabled_background", nvgRGB(201, 201, 209) },
    { "brls/button/primary_enabled_text", nvgRGB(255, 255, 255) },
    { "brls/button/primary_disabled_text", nvgRGB(220, 220, 228) },

    { "brls/button/default_enabled_background", nvgRGB(255, 255, 255) },
    { "brls/button/default_disabled_background", nvgRGB(255, 255, 255) },
    { "brls/button/default_enabled_text", nvgRGB(45, 45, 45) },
    { "brls/button/default_disabled_text", nvgRGB(45, 45, 45) },

    { "brls/button/highlight_enabled_text", nvgRGB(49, 79, 235) },
    { "brls/button/highlight_disabled_text", nvgRGB(49, 79, 235) },

    { "brls/button/enabled_border_color", nvgRGB(45, 45, 45) },
    { "brls/button/disabled_border_color", nvgRGB(45, 45, 45) },

    // List
    { "brls/list/listItem_value_color", nvgRGB(43, 81, 226) },

    // Slider
    { "brls/slider/pointer_color", nvgRGB(255, 255, 255) },
    { "brls/slider/pointer_border_color", nvgRGB(200, 200, 200) },
    { "brls/slider/line_filled", nvgRGB(50, 79, 241) },
    { "brls/slider/line_empty", nvgRGB(140, 140, 140) },

    // Spinner
    { "brls/spinner/bar_color", nvgRGBA(131, 131, 131, 80) },

};

static ThemeValues darkThemeValues = {
    // Generic values
    { "brls/clear", nvgRGB(45, 45, 45) },
    { "brls/background", nvgRGB(45, 45, 45) },
    { "brls/text", nvgRGB(255, 255, 255) },
    { "brls/text_disabled", nvgRGB(80, 80, 80) },
    { "brls/backdrop", nvgRGBA(0, 0, 0, 178) },
    { "brls/click_pulse", nvgRGBA(25, 138, 198, 38) }, // same as highlight color1 with different opacity
    { "brls/accent", nvgRGB(0, 255, 204) },

    // Highlight
    { "brls/highlight/background", nvgRGB(31, 34, 39) },
    { "brls/highlight/color1", nvgRGB(25, 138, 198) },
    { "brls/highlight/color2", nvgRGB(137, 241, 242) },

    // AppletFrame
    { "brls/applet_frame/separator", nvgRGB(255, 255, 255) },

    // Sidebar
    { "brls/sidebar/background", nvgRGB(50, 50, 50) },
    { "brls/sidebar/active_item", nvgRGB(0, 255, 204) },
    { "brls/sidebar/separator", nvgRGB(81, 81, 81) },

    // Header
    { "brls/header/border", nvgRGB(78, 78, 78) },
    { "brls/header/rectangle", nvgRGB(160, 160, 160) },
    { "brls/header/subtitle", nvgRGB(163, 163, 163) },

    // Button
    { "brls/button/primary_enabled_background", nvgRGB(1, 255, 201) },
    { "brls/button/primary_disabled_background", nvgRGB(83, 87, 86) },
    { "brls/button/primary_enabled_text", nvgRGB(52, 41, 55) },
    { "brls/button/primary_disabled_text", nvgRGB(71, 75, 74) },

    { "brls/button/default_enabled_background", nvgRGB(80, 80, 80) },
    { "brls/button/default_disabled_background", nvgRGB(80, 80, 80) },
    { "brls/button/default_enabled_text", nvgRGB(255, 255, 255) },
    { "brls/button/default_disabled_text", nvgRGB(255, 255, 255) },

    { "brls/button/highlight_enabled_text", nvgRGB(7, 247, 198) },
    { "brls/button/highlight_disabled_text", nvgRGB(7, 247, 198) },

    { "brls/button/enabled_border_color", nvgRGB(255, 255, 255) },
    { "brls/button/disabled_border_color", nvgRGB(255, 255, 255) },

    // List
    { "brls/list/listItem_value_color", nvgRGB(88, 195, 169) },

    // Slider
    { "brls/slider/pointer_color", nvgRGB(80, 80, 80) },
    { "brls/slider/pointer_border_color", nvgRGB(120, 120, 120) },
    { "brls/slider/line_filled", nvgRGB(1, 255, 201) },
    { "brls/slider/line_empty", nvgRGB(140, 140, 140) },

    // Spinner
    { "brls/spinner/bar_color", nvgRGBA(192, 192, 192, 80) }, // TODO: get this right
};

ThemeValues::ThemeValues(std::initializer_list<std::pair<std::string, NVGcolor>> list)
{
    for (std::pair<std::string, NVGcolor> color : list)
        this->values.insert(color);
}

void ThemeValues::addColor(const std::string& name, NVGcolor color)
{
        this->values[name] = color;
}

NVGcolor ThemeValues::getColor(const std::string& name)
{
    if (this->values.count(name) == 0)
        fatal("Unknown theme value \"" + name + "\" in size: " + std::to_string(this->values.size()));

    return this->values[name];
}

Theme::Theme(ThemeValues* values)
    : values(values)
{
}

NVGcolor Theme::getColor(const std::string& name)
{
    return this->values->getColor(name);
}

void Theme::addColor(const std::string& name, NVGcolor color)
{
    return this->values->addColor(name, color);
}

NVGcolor Theme::operator[](const std::string& name)
{
    return this->getColor(name);
}

Theme& Theme::getLightTheme()
{
    static Theme lightTheme(&lightThemeValues);
    return lightTheme;
}

Theme& Theme::getDarkTheme()
{
    static Theme darkTheme(&darkThemeValues);
    return darkTheme;
}

} // namespace brls
