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

class CircleButton : public brls::Box {
public:
    CircleButton();

    /** @brief 更新圆角和焦点高亮圆角 */
    void onLayout() override;

    /** @brief 获取内部图标 */
    brls::Image* getIcon();

    /**
     * @brief 设置选中态图标显示
     * @param selected 是否显示选中态图标
     */
    void setSelected(bool selected);

    /**
     * @brief 设置图标右上角角标显示
     * @param visible 是否显示角标
     */
    void setBadgeVisible(bool visible);

    /** @brief 创建圆形图标按钮 */
    static brls::View* create();

private:
    /** @brief 根据选中状态和主题更新图标 */
    void updateIcon();

    bool selected = false;             // 是否显示选中态图标
    std::string iconPath;              // 普通图标基础路径
    std::string selectedIconPath;      // 选中态图标完整路径

    BRLS_BIND(brls::Image, m_icon, "circleButton/icon");                 // 图标
    BRLS_BIND(brls::Box, m_iconContainer, "circleButton/iconContainer"); // 图标容器
    BRLS_BIND(brls::Box, m_badge, "circleButton/badge");                 // 右上角角标
};
