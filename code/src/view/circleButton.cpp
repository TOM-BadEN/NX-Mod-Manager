/**
 * CircleButton - 圆形图标按钮组件实现
 *
 * 参考 brls::Button 设计，使用 inflateFromXMLString 内联布局。
 * 构造时创建 50×50 圆形 Box + 居中 24×24 PNG 图标。
 */

#include "view/circleButton.hpp"

const std::string circleButtonXML = R"xml(
    <brls:Box
        width="50"
        height="50"
        axis="row"
        justifyContent="center"
        alignItems="center"
        focusable="true"
        backgroundColor="@theme/app/tagBg">

        <brls:Box
            id="circleButton/iconContainer"
            width="24"
            height="24">

            <brls:Image
                id="circleButton/icon"
                width="24"
                height="24"/>

            <brls:Box
                id="circleButton/badge"
                width="6"
                height="6"
                cornerRadius="3"
                backgroundColor="@theme/app/textWarning"
                positionType="absolute"
                positionTop="-3"
                positionRight="-3"
                visibility="invisible"/>

        </brls:Box>

    </brls:Box>
)xml";

CircleButton::CircleButton()
{
    this->inflateFromXMLString(circleButtonXML);

    // 触摸点击动画
    addGestureRecognizer(new brls::TapGestureRecognizer(this));

    // PNG 路径通过 XML 自定义属性传入（基础名，不带后缀）
    registerStringXMLAttribute("icon", [this](const std::string& baseName) {
        iconPath = baseName;
        updateIcon();
    });

    // 选中态 PNG 使用完整资源路径
    registerStringXMLAttribute("selectedIcon", [this](const std::string& value) {
        selectedIconPath = value;
        if (selected) updateIcon();
    });

    // 图标大小通过 XML 自定义属性传入（默认 24）
    registerFloatXMLAttribute("iconSize", [this](float size) {
        m_iconContainer->setWidth(size);
        m_iconContainer->setHeight(size);
        m_icon->setWidth(size);
        m_icon->setHeight(size);
    });
}

brls::Image* CircleButton::getIcon()
{
    return m_icon;
}

void CircleButton::setSelected(bool selected)
{
    this->selected = selected;
    updateIcon();
}

void CircleButton::setBadgeVisible(bool visible)
{
    m_badge->setVisibility(visible ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE);
}

void CircleButton::updateIcon()
{
    if (selected && !selectedIconPath.empty()) {
        m_icon->setImageFromRes(selectedIconPath);
        return;
    }

    if (iconPath.empty()) return;

    bool isDarkTheme = brls::Application::getThemeVariant() == brls::ThemeVariant::DARK;
    std::string suffix = isDarkTheme ? "-dark.png" : "-light.png";
    m_icon->setImageFromRes(iconPath + suffix);
}

void CircleButton::onLayout()
{
    Box::onLayout();
    float radius = getWidth() / 2;
    setCornerRadius(radius);
    setHighlightCornerRadius(radius);
}

brls::View* CircleButton::create()
{
    return new CircleButton();
}
