/**
 * SkeletonView - 通用轻量骨架组件
 *
 * 使用与 RecyclingGrid 骨架相同的主题色和呼吸效果。
 * 组件尺寸由使用方通过标准 XML 布局属性决定。
 *
 * XML 用法：
 *   <SkeletonView width="100%" height="145"/>
 */

#pragma once

#include <borealis.hpp>

class SkeletonView : public brls::Box {
public:
    SkeletonView();

    /** @brief 绘制轻量骨架呼吸效果 */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    /** @brief XML View 工厂函数 */
    static brls::View* create();

private:
    NVGcolor m_background = brls::Application::getTheme()["app/skeleton"]; // 骨架主题色
};
