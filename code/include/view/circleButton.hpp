/**
 * CircleButton - 圆形图标按钮组件
 *
 * 50×50 圆形，tagBg 背景，居中 PNG 图标。
 * 普通图标根据当前主题自动拼接 -dark.png / -light.png。
 * 内置 TapGestureRecognizer 支持触摸点击动画。
 *
 * XML 用法：
 *   <CircleButton icon="img/mod/like" marginLeft="20"/>
 *   <CircleButton icon="img/mod/like" selectedIcon="img/mod/like-done.png"/>
 *
 * icon 属性只写基础名（不带后缀），组件自动拼 -dark.png / -light.png。
 * selectedIcon 属性写完整 PNG 资源路径。
 */

#pragma once

#include <borealis.hpp>
#include <string>

class CircleButton : public brls::Box
{
  public:
    CircleButton();

    void onLayout() override;

    /** @brief 获取内部图标 */
    brls::Image* getIcon();

    /** @brief 设置选中态图标显示 */
    void setSelected(bool selected);

    /** @brief 设置图标右上角角标显示 */
    void setBadgeVisible(bool visible);

    static brls::View* create();

  private:
    void updateIcon();

    bool selected = false;
    std::string iconPath;
    std::string selectedIconPath;

    BRLS_BIND(brls::Image, m_icon, "circleButton/icon");
    BRLS_BIND(brls::Box, m_iconContainer, "circleButton/iconContainer");
    BRLS_BIND(brls::Box, m_badge, "circleButton/badge");
};
