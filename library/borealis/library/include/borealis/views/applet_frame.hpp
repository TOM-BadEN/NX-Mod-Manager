/*
    Copyright 2019-2021 natinusala
    Copyright 2021 XITRIX
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

#pragma once

#include <borealis/core/bind.hpp>
#include <borealis/core/box.hpp>
#include <borealis/core/event.hpp>
#include <borealis/views/bottom_bar.hpp>
#include <borealis/views/image.hpp>
#include <borealis/views/label.hpp>

namespace brls
{

enum class HeaderStyle
{
    REGULAR,
    POPUP
};

// A Horizon settings-like frame, with header and footer (no sidebar)
class AppletFrame : public Box
{
  public:
    AppletFrame();
    AppletFrame(View* contentView);

    ~AppletFrame()
    {
        brls::Logger::debug("delete AppletFrame {}", this->describe());
    }

    void handleXMLElement(tinyxml2::XMLElement* element) override;

    void pushContentView(View* view);
    void popContentView(std::function<void(void)> cb = [] {});

    void setTitle(std::string title);
    void setIcon(std::string path);

    void setHeaderVisibility(Visibility visibility);
    void setFooterVisibility(Visibility visibility);

    void setHeaderStyle(HeaderStyle style);
    void updateAppletFrameItem();

    View* getContentView()
    {
        return contentView;
    }

    Box* getHeader()
    {
        return header;
    }

    Box* getFooter()
    {
        return footer;
    }

    static View* create();

    inline static bool HIDE_BOTTOM_BAR = false;

  private:
    BRLS_BIND(Box, header, "brls/applet_frame/header");
    BRLS_BIND(BottomBar, footer, "brls/applet_frame/footer");
    BRLS_BIND(Label, title, "brls/applet_frame/title_label");
    BRLS_BIND(Image, icon, "brls/applet_frame/title_icon");
    BRLS_BIND(Box, hintBox, "brls/applet_frame/hint_box");

    HeaderStyle style = HeaderStyle::REGULAR;

  protected:
    std::vector<View*> contentViewStack;
    View* contentView = nullptr;

    /**
     * Sets the content view for that AppletFrame.
     * Will be placed between header and footer and expanded with grow factor
     * and width / height to AUTO.
     */
    void setContentView(View* view);
};

} // namespace brls
