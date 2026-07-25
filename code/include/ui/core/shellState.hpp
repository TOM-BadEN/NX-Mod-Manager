/**
 * ShellState - 当前页面提供给全局外壳的显示状态
 *
 * 只包含现阶段已经使用的标题、副标题和底部索引。页面通过 ShellState
 * 提供状态，不需要持有 AppShell、GlobalHeader 或 GlobalFooter 指针。
 */

#pragma once

#include <borealis.hpp>
#include <string>

/** @brief 当前页面的基础外壳显示状态 */
struct ShellStateData {
    std::string title;      // 顶部栏标题
    std::string subtitle;   // 顶部栏副标题，空字符串表示不显示
    std::string indexText;  // 底部栏索引文字，空字符串表示不显示
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
    /** @brief 设置当前页面的顶部栏标题 */
    void setTitle(std::string title);

    /** @brief 设置当前页面的顶部栏副标题，空字符串表示不显示 */
    void setSubtitle(std::string subtitle);

    /** @brief 设置当前页面的底部栏索引文字 */
    void setIndexText(std::string indexText);

private:
    ShellStateData m_state;                                 // 当前外壳显示状态
    brls::Event<const ShellStateData&> m_stateChangedEvent; // 外壳状态变化事件
};
