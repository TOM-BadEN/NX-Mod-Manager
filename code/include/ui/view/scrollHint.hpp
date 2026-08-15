/**
 * ScrollHint - 滚动内容底部提示遮罩
 *
 * 绘制融入卡片背景的底部渐变，只负责提示外观。
 * 显示条件和覆盖位置由调用页面控制。
 */

#pragma once

#include <borealis.hpp>

class ScrollHint : public brls::Box {
public:
    ScrollHint();

    /** @brief 设置渐变结束后保持不透明的底部高度 */
    void setSolidBottomHeight(float height);

    /** @brief 绘制底部两角圆角渐变 */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    /** @brief XML View 工厂函数 */
    static brls::View* create();

private:
    static constexpr float DEFAULT_CORNER_RADIUS = 8.0f;           // 默认底部圆角半径
    static constexpr float DEFAULT_SOLID_BOTTOM_HEIGHT = 40.0f;    // 默认不透明底部高度
    float m_solidBottomHeight = DEFAULT_SOLID_BOTTOM_HEIGHT;       // 当前不透明底部高度
};
