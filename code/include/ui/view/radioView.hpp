/**
 * RadioView - 单选指示器（外圈 + 选中圆点）
 *
 * 纯 NanoVG 自绘，不持有任何纹理；选中时圆点生长、外圈颜色渐变。
 */

#pragma once

#include <borealis.hpp>

class RadioView : public brls::View {
public:
    RadioView();

    /**
     * @brief 设置选中状态
     * @param selected 是否选中
     * @param animated 是否播放圆点生长动画
     */
    void setSelected(bool selected, bool animated = true);

    /**
     * @brief 设置是否可用（禁用时整体变暗）
     * @param enabled 是否可用
     */
    void setEnabled(bool enabled);

    /**
     * @brief 绘制单选指示器
     * @param vg NanoVG 上下文
     * @param x 横坐标
     * @param y 纵坐标
     * @param width 宽度
     * @param height 高度
     * @param style 当前样式
     * @param ctx 帧上下文
     */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

private:
    static constexpr float SIZE = 24.0f;        // 控件尺寸
    static constexpr float RING_STROKE = 2.0f;  // 外圈描边宽度
    static constexpr float DOT_RADIUS = 5.5f;   // 选中圆点半径

    bool m_enabled = true;                   // 是否可用
    brls::Animatable m_progress{0.0f};       // 选中进度（0=未选中，1=选中）
};
