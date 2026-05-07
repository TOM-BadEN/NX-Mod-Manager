/**
 * CircleButton - 圆形图标按钮组件
 *
 * 50×50 圆形，tagBg 背景，居中 SVGImage 图标。
 * 自动根据主题切换 SVG 颜色（dark/light）。
 * 内置 TapGestureRecognizer 支持触摸点击动画。
 *
 * XML 用法：
 *   <CircleButton SVG="img/mod/like" marginLeft="20"/>
 *
 * SVG 属性只写基础名（不带后缀），组件自动拼 -dark.svg / -light.svg。
 */

#pragma once

#include <borealis.hpp>
#include "view/svgImage.hpp"

class CircleButton : public brls::Box
{
  public:
    CircleButton();

    void onLayout() override;

    /** @brief 获取内部 SVG 图标 */
    SVGImage* getIcon();

    static brls::View* create();

  private:
    BRLS_BIND(SVGImage, m_icon, "circleButton/icon");
};
