/**
 * AddGameCard - 新增游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 MOD 数量（黑绿配色）
 */

#pragma once

#include "ui/view/recyclingGrid.hpp"
#include <borealis.hpp>
#include <string>

class AddGameCard : public RecyclingGridItem {
public:
    /** @brief 创建新增游戏卡片并加载 XML 布局 */
    AddGameCard();

    /** @brief 归还当前持有的游戏图标纹理引用 */
    ~AddGameCard() override;

    /** @brief 获得焦点时的回调 */
    void onFocusGained() override;

    /** @brief 失去焦点时的回调 */
    void onFocusLost() override;

    /**
     * @brief 绘制带焦点缩放效果的游戏图标
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
     * @param name 游戏名称
     * @param version 版本号
     * @param modCount MOD 数量
     */
    void setGame(const std::string& name, const std::string& version, const std::string& modCount);

    /**
     * @brief 设置游戏图标
     * @param iconId 已取得引用的 NVG 纹理 ID
     */
    void setIcon(int iconId);

    /** @brief 恢复默认图标 */
    void resetIcon();

    /** @brief 回收时重置状态 */
    void prepareForReuse() override;

    /** @brief 进入复用队列时归还游戏图标引用 */
    void cacheForReuse() override;

    /**
     * @brief 创建可供 RecyclingGrid 使用的新增游戏卡片
     * @return 新创建的新增游戏卡片
     */
    static RecyclingGridItem* create();

private:
    static constexpr float FOCUS_ICON_SCALE = 1.05f; // 游戏图标获得焦点后的缩放倍数

    int m_defaultIconId = 0;            // 默认游戏图标的纹理 ID
    int m_iconId = 0;                   // 当前持有引用的游戏图标纹理 ID
    brls::Animatable m_iconScale{1.0f}; // 游戏图标当前缩放值

    /**
     * @brief 根据焦点状态更新游戏图标缩放
     * @param focused 是否获得焦点
     */
    void updateFocusVisuals(bool focused);

    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "addGameCard/icon");         // 游戏图标
    BRLS_BIND(brls::Image, m_mask, "addGameCard/mask");         // 游戏图标底部渐变遮罩
    BRLS_BIND(brls::Image, m_like, "addGameCard/like");         // 收藏图标
    BRLS_BIND(brls::Label, m_name, "addGameCard/name");         // 游戏名称
    BRLS_BIND(brls::Label, m_version, "addGameCard/version");   // 游戏版本
    BRLS_BIND(brls::Label, m_modCount, "addGameCard/modCount"); // MOD 数量
};
