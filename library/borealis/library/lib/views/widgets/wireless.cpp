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

#include "borealis/views/widgets/wireless.hpp"

#include "borealis/core/thread.hpp"

#ifdef __SWITCH__
extern "C"
{
#include <switch/services/nifm.h>
}
#endif

namespace brls
{

WirelessWidget::WirelessWidget()
{
    platform = Application::getPlatform();
    if (!platform->canShowWirelessLevel())
        return;

    setSize(Size(44, 44));

    _0 = new Image();
    _0->setSize(Size(44, 44));
    _0->setScalingType(ImageScalingType::FIT);
    _0->detach();

    _1 = new Image();
    _1->setSize(Size(44, 44));
    _1->setScalingType(ImageScalingType::FIT);
    _1->detach();

    _2 = new Image();
    _2->setSize(Size(44, 44));
    _2->setScalingType(ImageScalingType::FIT);
    _2->detach();

    _3 = new Image();
    _3->setSize(Size(44, 44));
    _3->setScalingType(ImageScalingType::FIT);
    _3->detach();

    ethernet = new Image();
    ethernet->setSize(Size(44, 44));
    ethernet->setScalingType(ImageScalingType::FIT);
    ethernet->detach();

    platform = Application::getPlatform();
    applyTheme(platform->getThemeVariant());

    addView(_0);
    addView(_1);
    addView(_2);
    addView(_3);
    addView(ethernet);
}

void WirelessWidget::applyTheme(ThemeVariant theme)
{
    switch (theme)
    {
        case ThemeVariant::LIGHT:
            _0->setImageFromRes("img/sys/wifi_0_light.png");
            _1->setImageFromRes("img/sys/wifi_1_light.png");
            _2->setImageFromRes("img/sys/wifi_2_light.png");
            _3->setImageFromRes("img/sys/wifi_3_light.png");
            ethernet->setImageFromRes("img/sys/ethernet_light.png");
            break;
        case ThemeVariant::DARK:
            _0->setImageFromRes("img/sys/wifi_0_dark.png");
            _1->setImageFromRes("img/sys/wifi_1_dark.png");
            _2->setImageFromRes("img/sys/wifi_2_dark.png");
            _3->setImageFromRes("img/sys/wifi_3_dark.png");
            ethernet->setImageFromRes("img/sys/ethernet_dark.png");
            break;
    }
}

void WirelessWidget::updateState()
{
    static Time time = 0;
    Time now         = getCPUTimeUsec();
    if ((now - time) > 5000000 || time == 0)
    {
        brls::Logger::verbose("hasWirelessConnection: {}; wifiLevel: {}", hasWirelessConnection, wifiLevel);
#ifdef ANDROID
        hasEthernetConnection = Application::getPlatform()->hasEthernetConnection();
        hasWirelessConnection = Application::getPlatform()->hasWirelessConnection();
        wifiLevel             = Application::getPlatform()->getWirelessLevel();
#else
        ASYNC_RETAIN
        brls::async([ASYNC_TOKEN]()
            {
                ASYNC_RELEASE
#ifdef __SWITCH__
                // Reduce service calls
                // and fix support for emulator (Ryujinx) as it doesn't support :
                // nifmIsWirelessCommunicationEnabled() / nifmIsEthernetCommunicationEnabled().
                NifmInternetConnectionType type;
                u32 wifiSignal;
                NifmInternetConnectionStatus status;
                Result ret            = nifmGetInternetConnectionStatus(&type, &wifiSignal, &status);
                hasEthernetConnection = type == NifmInternetConnectionType_Ethernet;
                hasWirelessConnection = type == NifmInternetConnectionType_WiFi;
                if (ret != 0)
                {
                    hasEthernetConnection = false;
                    hasWirelessConnection = false;
                    wifiLevel             = 0;
                }
                else
                {
                    wifiLevel = (int)wifiSignal;
                }
#else
                hasEthernetConnection = Application::getPlatform()->hasEthernetConnection();
                hasWirelessConnection = Application::getPlatform()->hasWirelessConnection();
                wifiLevel             = Application::getPlatform()->getWirelessLevel();
#endif
            });
#endif
        time = now;
    }
}

void WirelessWidget::draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx)
{
    updateState();

    if (hasEthernetConnection)
    {
        _0->setVisibility(Visibility::GONE);
        _1->setVisibility(Visibility::GONE);
        _2->setVisibility(Visibility::GONE);
        _3->setVisibility(Visibility::GONE);
        ethernet->setVisibility(Visibility::VISIBLE);
    }
    else if (!hasWirelessConnection)
    {
        _0->setVisibility(Visibility::VISIBLE);
        _1->setVisibility(Visibility::GONE);
        _2->setVisibility(Visibility::GONE);
        _3->setVisibility(Visibility::GONE);
        ethernet->setVisibility(Visibility::GONE);
    }
    else
    {
        _0->setVisibility(Visibility::GONE);
        _1->setVisibility(Visibility::VISIBLE);
        _2->setVisibility(Visibility::VISIBLE);
        _3->setVisibility(Visibility::VISIBLE);
        ethernet->setVisibility(Visibility::GONE);

        switch (wifiLevel)
        {
            case 0:
                _1->setAlpha(0.2f);
                _2->setAlpha(0.2f);
                _3->setAlpha(0.2f);
                break;
            case 1:
                _1->setAlpha(1);
                _2->setAlpha(0.2f);
                _3->setAlpha(0.2f);
                break;
            case 2:
                _1->setAlpha(1);
                _2->setAlpha(1);
                _3->setAlpha(0.2f);
                break;
            default:
                _1->setAlpha(1);
                _2->setAlpha(1);
                _3->setAlpha(1);
                break;
        }
    }

    Box::draw(vg, x, y, width, height, style, ctx);
}

View* WirelessWidget::create()
{
    return new WirelessWidget();
}

} // namespace brls
