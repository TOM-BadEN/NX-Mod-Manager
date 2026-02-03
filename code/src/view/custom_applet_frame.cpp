/**
 * CustomAppletFrame 实现
 */

#include "view/custom_applet_frame.hpp"
#include <borealis/views/widgets/battery.hpp>
#include <borealis/views/widgets/wireless.hpp>
#include <borealis/views/hint.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

const std::string customAppletFrameXML = R"xml(
    <brls:Box
        width="auto"
        height="auto"
        axis="column"
        justifyContent="spaceBetween">

        <!-- Header -->
        <brls:Box
            id="custom/header"
            width="auto"
            height="@style/brls/applet_frame/header_height"
            axis="row"
            justifyContent="spaceBetween"
            alignItems="center"
            paddingLeft="@style/brls/applet_frame/header_padding_sides"
            paddingRight="@style/brls/applet_frame/header_padding_sides"
            marginLeft="@style/brls/applet_frame/padding_sides"
            marginRight="@style/brls/applet_frame/padding_sides"
            lineColor="@theme/brls/applet_frame/separator"
            lineBottom="1px">

            <!-- 左侧：图标 + 标题 -->
            <brls:Box
                width="auto"
                height="auto"
                axis="row"
                alignItems="center"
                marginTop="8">
                <brls:Image
                    id="custom/icon"
                    width="auto"
                    height="auto"
                    marginRight="@style/brls/applet_frame/header_image_title_spacing"
                    visibility="gone" />

                <brls:Label
                    id="custom/title"
                    width="auto"
                    height="auto"
                    fontSize="@style/brls/applet_frame/header_title_font_size" />
            </brls:Box>

            <!-- 右侧：时间 WiFi xx% 电池 -->
            <brls:Box
                width="auto"
                height="auto"
                axis="row"
                alignItems="center"
                marginTop="13">

                <brls:Label
                    id="custom/time"
                    width="auto"
                    height="auto"
                    marginRight="20"
                    fontSize="21.5"/>

                <brls:Wireless
                    marginRight="20"
                    marginBottom="5"/>

                <brls:Label
                    id="custom/battery_percent"
                    width="auto"
                    height="auto"
                    marginRight="5"
                    fontSize="21.5"/>

                <brls:Battery
                    marginBottom="5"/>

            </brls:Box>

        </brls:Box>

        <!-- Content 内容区域 -->
        <brls:Box
            id="custom/content"
            width="auto"
            height="auto"
            grow="1"/>

        <!-- Footer 底部按钮提示 -->
        <brls:Box
            width="auto"
            height="@style/brls/applet_frame/footer_height"
            axis="row"
            justifyContent="flexEnd"
            alignItems="center"
            marginLeft="@style/brls/applet_frame/padding_sides"
            marginRight="@style/brls/applet_frame/padding_sides"
            paddingLeft="@style/brls/hints/footer_padding_sides"
            paddingRight="@style/brls/hints/footer_padding_sides"
            paddingTop="@style/brls/hints/footer_padding_top_bottom"
            paddingBottom="@style/brls/hints/footer_padding_top_bottom"
            lineColor="@theme/brls/applet_frame/separator"
            lineTop="1px">

            <brls:Hints
                width="auto"
                height="auto"/>

        </brls:Box>

    </brls:Box>
)xml";

CustomAppletFrame::CustomAppletFrame()
{
    this->inflateFromXMLString(customAppletFrameXML);
    
    this->registerAction(
        "返回", brls::BUTTON_B, [](brls::View* view) {
            if (!brls::Application::popActivity())
            {
                // 已经是最后一个 Activity，弹出退出确认对话框
                auto dialog = new brls::Dialog("确定要退出吗？");
                dialog->addButton("取消", []() {});
                dialog->addButton("确定", []() { brls::Application::quit(); });
                dialog->open();
            }
            return true;
        },
        false, false, brls::SOUND_BACK);
}

void CustomAppletFrame::handleXMLElement(tinyxml2::XMLElement* element)
{
    if (this->contentView)
        brls::fatal("CustomAppletFrame can only have one child XML element");

    brls::View* view = brls::View::createFromXMLElement(element);
    this->setContentView(view);
}

void CustomAppletFrame::setContentView(brls::View* view)
{
    if (this->contentView)
    {
        this->contentBox->removeView(this->contentView, false);
        this->contentView = nullptr;
    }

    if (!view)
        return;

    this->contentView = view;
    this->contentView->setDimensions(brls::View::AUTO, brls::View::AUTO);
    this->contentView->setGrow(1.0f);
    this->contentBox->addView(this->contentView);
    
    this->updateAppletFrameItem();
}

void CustomAppletFrame::setTitle(std::string title)
{
    this->titleLabel->setText(title);
}

void CustomAppletFrame::setIcon(std::string path)
{
    if (path.empty())
    {
        this->iconImage->setVisibility(brls::Visibility::GONE);
    }
    else
    {
        this->iconImage->setVisibility(brls::Visibility::VISIBLE);
        this->iconImage->setImageFromFile(path);
    }
}

void CustomAppletFrame::updateAppletFrameItem()
{
    if (!this->contentView)
        return;
        
    setTitle(contentView->getAppletFrameItem()->title);
    setIcon(contentView->getAppletFrameItem()->iconPath);
}

void CustomAppletFrame::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx)
{
    this->updateStatusText();
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

void CustomAppletFrame::updateStatusText()
{
    // 更新时间
    auto timeNow = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(timeNow);
    auto tm = *std::localtime(&in_time_t);
    std::stringstream ss;
    ss << std::put_time(&tm, "%H:%M");
    if (ss.str() != this->timeText)
    {
        this->timeText = ss.str();
        this->timeLabel->setText(this->timeText);
        
        // 更新电量百分比
        auto platform = brls::Application::getPlatform();
        if (platform->canShowBatteryLevel())
        {
            int level = platform->getBatteryLevel();
            this->batteryPercentLabel->setText(std::to_string(level) + "%");
        }
    }
}

brls::View* CustomAppletFrame::create()
{
    return new CustomAppletFrame();
}
