/**
 * ContextMenuCell - 上下文菜单单元格
 *
 * 只负责显示标题、图标以及唯一一种右侧内容。
 */

#pragma once

#include "ui/view/radioView.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "ui/view/switchView.hpp"
#include <borealis.hpp>
#include <string>

class ContextMenuCell final : public RecyclingGridItem {
public:
    /** @brief 创建上下文菜单单元格 */
    ContextMenuCell();

    /** @brief 获得焦点时的回调 */
    void onFocusGained() override;

    /** @brief 失去焦点时的回调 */
    void onFocusLost() override;

    /**
     * @brief 绘制带焦点缩放效果的菜单项图标
     * @param vg NanoVG 上下文
     * @param x 单元格横坐标
     * @param y 单元格纵坐标
     * @param width 单元格宽度
     * @param height 单元格高度
     * @param style 当前界面样式
     * @param ctx 当前帧上下文
     */
    void draw(NVGcontext* vg, float x, float y, float width, float height,
        brls::Style style, brls::FrameContext* ctx) override;

    /**
     * @brief 完整绑定公共显示内容，并清理上一次绑定状态
     * @param title 菜单项标题
     * @param icon 图标资源路径，空字符串表示不显示图标
     * @param disabled 菜单项是否禁用
     */
    void bindBase(const std::string& title, const std::string& icon, bool disabled);

    /**
     * @brief 显示徽章
     * @param badge 徽章文字，空字符串表示不显示徽章
     * @param highlighted 是否使用高亮颜色
     */
    void showBadge(const std::string& badge, bool highlighted);

    /**
     * @brief 显示开关
     * @param state 当前真实状态
     * @param animated 是否播放状态切换动画
     * @param previousState 动画开始前的状态
     */
    void showSwitch(bool state, bool animated = false, bool previousState = false);

    /**
     * @brief 显示单选指示器
     * @param selected 当前真实选中状态
     * @param animated 是否播放状态切换动画
     * @param previousState 动画开始前的状态
     */
    void showRadio(bool selected, bool animated = false, bool previousState = false);

    /** @brief 显示后台任务加载动画 */
    void showLoading();

    /**
     * @brief 查询开关是否仍在播放切换动画
     * @return 正在播放动画时返回 true，否则返回 false
     */
    bool isSwitchAnimating() const;

    /** @brief 回收复用前彻底清理显示状态 */
    void prepareForReuse() override;

    /**
     * @brief RecyclingGrid Cell 工厂
     * @return 新创建的上下文菜单单元格
     */
    static RecyclingGridItem* create();

private:
    static constexpr float FOCUS_ICON_SCALE = 1.10f; // 图标获得焦点后的缩放倍数

    brls::Animatable m_iconScale{1.0f}; // 图标当前缩放值

    /**
     * @brief 根据焦点状态更新图标缩放
     * @param focused 是否获得焦点
     */
    void updateFocusVisuals(bool focused);

    /** @brief 清理全部可复用显示状态 */
    void resetContent();

    /**
     * @brief 应用标题和徽章颜色
     * @param badgeHighlighted 徽章是否使用高亮颜色
     */
    void applyColors(bool badgeHighlighted);

    bool m_disabled = false; // 当前单元格是否禁用
    bool m_hasIcon = false;  // 当前单元格是否配置图标

    BRLS_BIND(brls::Image, m_icon, "contextMenuCell/icon");
    BRLS_BIND(brls::Label, m_title, "contextMenuCell/title");
    BRLS_BIND(brls::Label, m_badge, "contextMenuCell/badge");
    BRLS_BIND(brls::ProgressSpinner, m_loading, "contextMenuCell/loading");
    BRLS_BIND(brls::Box, m_rightBox, "contextMenuCell/right");

    SwitchView* m_switch = nullptr; // 开关指示器，由 m_rightBox 持有
    RadioView* m_radio = nullptr;   // 单选指示器，由 m_rightBox 持有
};
