/**
 * GameCard - 游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 mod 数量
 */

#pragma once

#include <borealis.hpp>
#include <functional>
#include "view/recyclingGrid.hpp"
#include "view/capsuleBadge.hpp"

class GameCard : public RecyclingGridItem {
public:
    GameCard();

    /** @brief 获得焦点时的回调 */
    void onFocusGained() override;

    /** @brief 失去焦点时的回调 */
    void onFocusLost() override;

    /** @brief 绘制带启动按压缩放效果的游戏图标 */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    
    /**
     * @brief 设置卡片数据
     * @param name 游戏名称
     * @param version 版本号
     * @param modCount mod 数量
     */
    void setGame(const std::string& name, const std::string& version, const std::string& modCount);

    /** @brief 设置是否显示启动提示 */
    void setLaunchAvailable(bool available);

    /** @brief 设置启动动画完成后的回调 */
    void setLaunchAction(std::function<void()> action);
    
    /**
     * @brief 设置游戏图标
     * @param iconId NVG 纹理 ID
     */
    void setIcon(int iconId);
    
    /**
     * @brief 设置收藏状态
     * @param favorite 是否收藏
     */
    void setFavorite(bool favorite);

    /**
     * @brief 设置游戏名文字颜色
     * @param color 颜色
     */
    void setNameColor(NVGcolor color);
    
    /** @brief 恢复默认图标 */
    void resetIcon();

    /** @brief 回收时重置状态 */
    void prepareForReuse() override;

    /** @brief 工厂函数 */
    static RecyclingGridItem* create();

private:
    static constexpr float LAUNCH_HINT_HIDDEN_X = 106.0f;       // 启动提示完全移出图标区域时的横向偏移

    int m_defaultIconId = 0;                                    // 默认游戏图标的纹理 ID
    bool m_launchAvailable = false;                             // 当前游戏是否已安装并允许显示启动提示
    brls::Animatable m_launchHintX{LAUNCH_HINT_HIDDEN_X};       // 启动提示横向平移动画值
    std::function<void()> m_launchAction;                       // 启动动画完成后的回调
    brls::Animatable m_launchScale{1.0f};                       // 启动按下时的游戏图标缩放值

    void animateLaunchHint(bool visible);                       // 显示或立即隐藏启动提示
    void animateLaunchPress();                                  // 播放启动按下并回弹的缩放动画

    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "gameCard/icon");
    BRLS_BIND(brls::Image, m_like, "gameCard/like");
    BRLS_BIND(brls::Box, m_launchHint, "gameCard/launchHint");
    BRLS_BIND(brls::Label, m_launchIcon, "gameCard/launchIcon");
    BRLS_BIND(brls::Label, m_launchText, "gameCard/launchText");
    BRLS_BIND(brls::Label, m_name, "gameCard/name");
    BRLS_BIND(CapsuleBadge, m_version, "gameCard/version");
    BRLS_BIND(CapsuleBadge, m_modCount, "gameCard/modCount");
};
