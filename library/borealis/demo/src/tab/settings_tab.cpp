/*
    Copyright 2021 natinusala

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

#include "tab/settings_tab.hpp"

using namespace brls::literals;  // for _i18n

bool radioSelected = false;

static std::vector<std::string> NOTIFICATIONS = {
    "You have cool hair",
    "I like your shoes",
    "borealis is powered by nanovg",
    "The Triforce is an inside job",
    "Pozznx will trigger in one day and twelve hours",
    "Aurora Borealis? At this time of day, at this time of year, in this part of the gaming market, located entirely within your Switch?!",
    "May I see it?",
    "Hmm, Steamed Hams!",
    "Hello\nWorld!"
};

SettingsTab::SettingsTab()
{
    // Inflate the tab from the XML file
    this->inflateFromXMLRes("xml/tabs/settings.xml");

    radio->title->setText("Radio cell");
    radio->setSelected(radioSelected);
    radio->registerClickAction([this](brls::View* view) {
        radioSelected = !radioSelected;
        this->radio->setSelected(radioSelected);
        return true;
    });

    boolean->title->setText("Switcher");

    debug->init("Debug Layer", brls::Application::isDebuggingViewEnabled(), [](bool value){
        brls::Application::enableDebuggingView(value);
        brls::sync([value](){
            brls::Logger::info("{} the debug layer", value ? "Open" : "Close");
        });
    });

    screenSaver->init("Disable ScreenSaver", brls::Application::getPlatform()->isScreenDimmingDisabled(), [](bool value){
        brls::Application::getPlatform()->disableScreenDimming(value);
    });

    bottomBar->init("Bottom Bar", !brls::AppletFrame::HIDE_BOTTOM_BAR, [](bool value){
        brls::AppletFrame::HIDE_BOTTOM_BAR = !value;
        auto stack = brls::Application::getActivitiesStack();
        for (auto& activity : stack) {
            auto* frame = dynamic_cast<brls::AppletFrame*>(
                activity->getContentView());
            if (!frame) continue;
            frame->setFooterVisibility(!value ? brls::Visibility::GONE
                                             : brls::Visibility::VISIBLE);
        }
    });

    fps->init("FPS", brls::Application::getFPSStatus(), [](bool value){
        brls::Application::setFPSStatus(value);
    });

    swapInterval->init("Swap Interval", {"0", "1", "2", "3", "4"}, 1, [](int selected) {},
                       [](int selected) { brls::Application::setSwapInterval(selected); });

    alwaysOnTop->init("Always On Top", false, [](bool value){
        brls::Application::getPlatform()->setWindowAlwaysOnTop(value);
    });

    selector->init("Selector", { "Test 1", "Test 2", "Test 3", "Test 4", "Test 5", "Test 6", "Test 7", "Test 8", "Test 9", "Test 10", "Test 11", "Test 12", "Test 13" }, 0, [](int selected) {
    }, [](int selected) {
        auto dialog = new brls::Dialog(fmt::format("selected {}", selected));
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
    });

    input->init(
        "Input text", "https://github.com", [](std::string text) {

        },
        "Placeholder", "Hint");

    inputNumeric->init(
        "Input number", 2448, [](int number) {

        },
        "Hint");

    ipAddress->setDetailText(brls::Application::getPlatform()->getIpAddress());
    dnsServer->setDetailText(brls::Application::getPlatform()->getDnsServer());

    input->registerAction("hints/open"_i18n, brls::BUTTON_X, [](brls::View* view) {
        brls::DetailCell *cell = dynamic_cast<brls::DetailCell *>(view);
        brls::Application::getPlatform()->openBrowser(cell->detail->getFullText());
        return true;
    }, false, false, brls::SOUND_CLICK);

    float brightness = brls::Application::getPlatform()->getBacklightBrightness();
    slider->init("Brightness", brightness, [this](float value){
        brls::Application::getPlatform()->setBacklightBrightness(value);
        slider->setDetailText(fmt::format("{:.2f}", value));
    });
    slider->setDetailText(fmt::format("{:.2f}", brightness));

    notify->registerClickAction([](...){
        std::string notification = NOTIFICATIONS[std::rand() % NOTIFICATIONS.size()];
        brls::Application::notify(notification);
        return true;
    });
}

brls::View* SettingsTab::create()
{
    return new SettingsTab();
}
