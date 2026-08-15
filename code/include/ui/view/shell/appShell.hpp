/**
 * AppShell - 全局常驻 UI 外壳
 *
 * 组合 GlobalHeader、PageHost 和 GlobalFooter，并将当前页面提供的
 * ShellState 应用到常驻顶部栏和底部栏。
 */

#pragma once

#include "ui/core/pageHost.hpp"
#include "ui/core/shellState.hpp"
#include "ui/view/gradientBox.hpp"
#include "ui/view/shell/globalFooter.hpp"
#include "ui/view/shell/globalHeader.hpp"
#include <borealis.hpp>

class AppShell : public GradientBox {
public:
    AppShell();
    ~AppShell() override;

    /** @brief 获取普通页面内容容器 */
    PageHost* getPageHost() const;

    /**
     * @brief 设置全局标题栏是否读取并显示 FPS
     * @param show 是否读取并显示 FPS
     */
    void setShowFps(bool show);

    /**
     * @brief 设置全局标题栏是否显示并读取内存使用情况
     * @param show 是否显示并读取内存使用情况
     */
    void setShowMem(bool show);

    /**
     * @brief 播放全局标题栏导航边界抖动动画
     * @param right 是否向右抖动，false 表示向左
     */
    void shakeHeaderNav(bool right);

    /** @brief XML View 工厂函数 */
    static brls::View* create();

private:
    GlobalHeader* m_header = nullptr;   // 全局常驻顶部栏
    PageHost* m_pageHost = nullptr;     // 普通页面内容容器
    GlobalFooter* m_footer = nullptr;   // 全局常驻底部栏
    ShellState* m_shellState = nullptr; // 当前页面的外壳状态

    brls::Event<Page*>::Subscription m_pageChangedSubscription;                  // 当前页面变化订阅
    brls::Event<const ShellStateData&>::Subscription m_stateChangedSubscription; // 当前页面外壳状态订阅

    /** @brief 切换到当前页面对应的外壳状态 */
    void bindShellState(Page* page);

    /** @brief 将当前页面的标题状态和索引立即应用到全局外壳 */
    void applyShellState(const ShellStateData& state);
};
