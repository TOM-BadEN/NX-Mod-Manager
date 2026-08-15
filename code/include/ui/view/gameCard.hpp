/**
 * GameCard - 游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 mod 数量
 */

#pragma once

#include "ui/view/recyclingGrid.hpp"
#include <borealis.hpp>
#include <functional>
#include <string>

class GameCard : public RecyclingGridItem {
public:
    /** @brief 创建游戏卡片并加载 XML 布局 */
    GameCard();

    /** @brief 析构前恢复由卡片持有的默认图标 */
    ~GameCard() override;

    /** @brief 获得焦点时的回调 */
    void onFocusGained() override;

    /** @brief 失去焦点时的回调 */
    void onFocusLost() override;

    /**
     * @brief 绘制带焦点和启动按压缩放效果的游戏图标
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
     * @param modCount mod 数量
     */
    void setGame(const std::string& name, const std::string& version, const std::string& modCount);

    /**
     * @brief 设置是否显示启动提示
     * @param available 是否允许启动游戏
     */
    void setLaunchAvailable(bool available);

    /**
     * @brief 设置启动动画完成后的回调
     * @param action 启动回调
     */
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

    /**
     * @brief 创建可供 RecyclingGrid 使用的游戏卡片
     * @return 新创建的游戏卡片
     */
    static RecyclingGridItem* create();

private:
    static constexpr float LAUNCH_HINT_HIDDEN_X = 106.0f; // 启动提示完全移出图标区域时的横向偏移
    static constexpr float FOCUS_ICON_SCALE = 1.05f;      // 游戏图标获得焦点后的缩放倍数
    static constexpr float PRESSED_ICON_SCALE = FOCUS_ICON_SCALE * 0.95f; // 启动按下时的游戏图标缩放倍数

    int m_defaultIconId = 0;                              // 默认游戏图标的纹理 ID
    bool m_launchAvailable = false;                       // 当前游戏是否已安装并允许显示启动提示
    bool m_launching = false;                             // 是否正在播放启动按压动画
    brls::Animatable m_launchHintX{LAUNCH_HINT_HIDDEN_X}; // 启动提示横向平移动画值
    std::function<void()> m_launchAction;                 // 启动动画完成后的回调
    brls::Animatable m_iconScale{1.0f};                   // 游戏图标当前缩放值

    /**
     * @brief 更新启动提示显示状态
     * @param visible 是否显示启动提示
     */
    void updateLaunchHint(bool visible);

    /** @brief 播放启动按下并回弹的缩放动画 */
    void animateLaunchPress();

    /**
     * @brief 根据焦点状态更新游戏图标和启动提示
     * @param focused 是否获得焦点
     */
    void updateFocusVisuals(bool focused);

    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "gameCard/icon");                  // 游戏图标
    BRLS_BIND(brls::Image, m_mask, "gameCard/mask");                  // 游戏图标底部渐变遮罩
    BRLS_BIND(brls::Image, m_like, "gameCard/like");                  // 收藏图标
    BRLS_BIND(brls::Box, m_launchHint, "gameCard/launchHint");        // 启动提示容器
    BRLS_BIND(brls::Label, m_launchIcon, "gameCard/launchIcon");      // 启动按键图标
    BRLS_BIND(brls::Label, m_launchText, "gameCard/launchText");      // 启动提示文本
    BRLS_BIND(brls::Label, m_name, "gameCard/name");                  // 游戏名称
    BRLS_BIND(brls::Label, m_version, "gameCard/version");            // 游戏版本
    BRLS_BIND(brls::Label, m_modCount, "gameCard/modCount");          // MOD 数量
};
