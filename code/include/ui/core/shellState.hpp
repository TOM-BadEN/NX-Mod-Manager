/**
 * ShellState - 当前页面提供给全局外壳的显示状态
 *
 * 包含顶部标题区域、底部索引和底栏背景的显示状态。页面通过 ShellState 提供状态，
 * 不需要持有 AppShell、GlobalHeader 或 GlobalFooter 指针。
 */

#pragma once

#include <borealis.hpp>
#include "ui/core/headerState.hpp"
#include <string>

/** @brief 当前页面的完整外壳显示状态 */
struct ShellStateData {
    HeaderState headerState; // 顶部标题区域状态
    std::string indexText; // 底部栏索引文字，空字符串表示不显示
    std::string footerBackgroundTheme; // 底部栏背景主题色，空字符串表示透明
};

/**
 * @brief 页面外壳状态
 *
 * 需要更新全局外壳的 Page 可以额外继承本类。AppShell 只订阅当前页面
 * 的状态变化，非当前页面不会继续影响顶部栏和底部栏。
 */
class ShellState {
public:
    virtual ~ShellState();

    /** @brief 获取当前外壳显示状态 */
    const ShellStateData& getState() const;

    /** @brief 获取外壳状态变化事件 */
    brls::Event<const ShellStateData&>* getStateChangedEvent();

protected:
    /** @brief 设置当前页面的完整标题状态 */
    void setHeaderState(HeaderState state);

    /** @brief 单独设置当前页面的普通标题 */
    void setHeaderTitle(TitleState title);

    /** @brief 单独设置当前页面的内容标题 */
    void setHeaderContentTitle(std::string contentTitle);

    /** @brief 设置当前页面的底部栏索引文字 */
    void setIndexText(std::string indexText);

    /** @brief 设置当前页面的底部栏背景主题色 */
    void setFooterBackgroundTheme(std::string themeKey);

private:
    ShellStateData m_state;                                 // 当前外壳显示状态
    brls::Event<const ShellStateData&> m_stateChangedEvent; // 外壳状态变化事件
};
