/**
 * ModCard - Mod 卡片组件
 * 显示单个 mod 的图标、名称、类型和安装状态
 */

#pragma once

#include "ui/view/recyclingGrid.hpp"

class ModCard : public RecyclingGridItem {
public:
    ModCard();

    /** @brief 获得焦点时的回调 */
    void onFocusGained() override;

    /** @brief 失去焦点时的回调 */
    void onFocusLost() override;

    /**
     * @brief 绘制带焦点缩放效果的模组图标
     * @param vg NanoVG 上下文
     * @param x 卡片横坐标
     * @param y 卡片纵坐标
     * @param width 卡片宽度
     * @param height 卡片高度
     * @param style 当前界面样式
     * @param ctx 当前帧上下文
     */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    /**
     * @brief 设置卡片数据
     * @param name 模组名称
     * @param type 类型
     * @param installed 是否已安装
     * @param disabled 游戏是否处于模组禁用状态
     * @param modID 模组 ID
     * @param hasUpdate 是否存在可用更新
     */
    void setMod(const std::string& name, const std::string& type, bool installed, bool disabled, int modID = -1, bool hasUpdate = false);
    
    /** @brief 回收复用时重置内容 */
    void prepareForReuse() override;

    /** @brief 工厂函数 */
    static RecyclingGridItem* create();

private:
    static constexpr float FOCUS_ICON_SCALE = 1.10f; // 模组图标获得焦点后的缩放倍数

    brls::Animatable m_iconScale{1.0f}; // 模组图标当前缩放值

    /**
     * @brief 根据焦点状态更新模组图标缩放
     * @param focused 是否获得焦点
     */
    void updateFocusVisuals(bool focused);

    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "modCard/icon");
    BRLS_BIND(brls::Label, m_name, "modCard/name");
    BRLS_BIND(brls::Label, m_type, "modCard/type");
    BRLS_BIND(brls::Label, m_status, "modCard/status");
    BRLS_BIND(brls::Image, m_cloudIcon, "modCard/cloudIcon");
    BRLS_BIND(brls::Box, m_updateBadge, "modCard/updateBadge");
};
