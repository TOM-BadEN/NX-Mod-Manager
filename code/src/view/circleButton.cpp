/**
 * CircleButton - 圆形图标按钮组件实现
 *
 * 参考 brls::Button 设计，使用 inflateFromXMLString 内联布局。
 * 构造时创建 50×50 圆形 Box + 居中 24×24 SVGImage。
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

        <SVGImage
            id="circleButton/icon"
            width="24"
            height="24"/>

    </brls:Box>
)xml";

CircleButton::CircleButton()
{
    this->inflateFromXMLString(circleButtonXML);

    // 触摸点击动画
    addGestureRecognizer(new brls::TapGestureRecognizer(this));

    // SVG 路径通过 XML 自定义属性传入（基础名，不带后缀）
    registerStringXMLAttribute("SVG", [this](const std::string& baseName) {
        bool isDarkTheme = brls::Application::getThemeVariant() == brls::ThemeVariant::DARK;
        std::string suffix = isDarkTheme ? "-dark.svg" : "-light.svg";
        m_icon->setImageFromSVGRes(baseName + suffix);
    });

    // 图标大小通过 XML 自定义属性传入（默认 24）
    registerFloatXMLAttribute("iconSize", [this](float size) {
        m_icon->setWidth(size);
        m_icon->setHeight(size);
    });
}

SVGImage* CircleButton::getIcon()
{
    return m_icon;
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
