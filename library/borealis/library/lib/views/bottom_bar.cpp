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

#include <borealis/views/bottom_bar.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef PS4
#include <borealis/platforms/ps4/ps4_sysmodule.hpp>
#endif

namespace brls
{

const std::string bottomBarXML = R"xml(

<brls:Box
    width="auto"
    height="@style/brls/applet_frame/footer_height"
    axis="column">
    <brls:Box
        width="auto"
        height="@style/brls/applet_frame/footer_height"
        marginLeft="@style/brls/hints/footer_margin_sides"
        marginRight="@style/brls/hints/footer_margin_sides"
        paddingLeft="@style/brls/hints/footer_padding_sides"
        paddingRight="@style/brls/hints/footer_padding_sides"
        lineColor="@theme/brls/applet_frame/separator"
        lineTop="1px"
        alignItems="stretch">
        <brls:Box
            width="auto"
            height="@style/brls/applet_frame/footer_height"
            axis="row"
            grow="1"
            direction="rightToLeft"
            justifyContent="spaceBetween"
            paddingTop="@style/brls/hints/footer_padding_top_bottom"
            paddingBottom="@style/brls/hints/footer_padding_top_bottom" >

            <brls:Hints
                width="auto"
                height="auto"/>

            <brls:Box
                width="auto"
                height="auto"
                axis="row"
                alignItems="center"
                direction="leftToRight" >

                <brls:Battery
                    id="brls/battery"
                    marginRight="21"
                    marginBottom="5"/>

                <brls:Wireless
                    id="brls/wireless"
                    marginRight="21"
                    marginBottom="5"/>

                <brls:Label
                    id="brls/hints/time"
                    width="auto"
                    height="auto"
                    verticalAlign="center"
                    fontSize="21.5" />

            </brls:Box>

        </brls:Box>
    </brls:Box>
</brls:Box>
)xml";

BottomBar::BottomBar()
{
    this->inflateFromXMLString(bottomBarXML);

    Platform* platform = Application::getPlatform();
    battery->setVisibility(platform->canShowBatteryLevel() ? Visibility::VISIBLE : Visibility::GONE);
    wireless->setVisibility(platform->canShowWirelessLevel() ? Visibility::VISIBLE : Visibility::GONE);
}

void BottomBar::draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx)
{
    this->updateText();
    Box::draw(vg, x, y, width, height, style, ctx);
}

void BottomBar::updateText()
{
#ifdef PS4
    OrbisDateTime lt{};
    if (sceRtcGetCurrentClockLocalTime)
        sceRtcGetCurrentClockLocalTime(&lt);
    tm tm{};
    tm.tm_hour = lt.hour;
    tm.tm_min = lt.minute;
    tm.tm_sec = lt.second;
#else
    auto timeNow   = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(timeNow);
    auto tm = *std::localtime(&in_time_t);
#endif
    std::stringstream ss;
    ss << std::put_time(&tm, "%H:%M:%S");
    if (ss.str() != bottomText)
    {
        bottomText = ss.str();
        if (Application::getFPSStatus())
        {
            time->setText(bottomText + " | FPS:" + std::to_string(Application::getFPS()));
        }
        else
        {
            time->setText(bottomText);
        }
    }
}

View* BottomBar::create()
{
    return new BottomBar();
}

} // namespace brls
