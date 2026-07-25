/**
 * Search - 搜索页面
 *
 * 在全局 UI Shell 的中间内容区域显示虚拟键盘和搜索结果。
 */

#pragma once

#include "ui/core/page.hpp"
#include "ui/core/shellState.hpp"
#include "ui/view/keyboard.hpp"
#include "utils/searchEngine.hpp"
#include <borealis.hpp>
#include <functional>
#include <string>
#include <vector>

/** @brief 搜索结果按钮 */
class ResultButton : public brls::Box {
public:
    /** @brief 获得焦点时恢复高亮和点击动画 */
    void onFocusGained() override;

    /** @brief 失去焦点时清理动画状态 */
    void onFocusLost() override;
};

/** @brief 搜索结果按钮网格容器 */
class ResultButtonGrid : public brls::Box {
public:
    /**
     * @brief 设置当前搜索结果按钮列表
     * @param buttons 搜索结果按钮列表
     */
    void setButtons(std::vector<ResultButton*>* buttons);

    /**
     * @brief 根据方向查找下一个搜索结果焦点
     * @param direction 焦点移动方向
     * @param currentView 当前参与导航的 View
     * @return 下一个焦点 View
     */
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    /** @brief XML View 工厂函数 */
    static brls::View* create();

private:
    std::vector<ResultButton*>* m_buttons = nullptr; // 当前搜索结果按钮列表
};

/** @brief 通用搜索页面 */
class Search : public Page, public ShellState {
public:
    /**
     * @brief 创建搜索页面
     * @param items 搜索数据源
     * @param onSelect 搜索结果选中回调
     */
    Search(const std::vector<std::string>& items, std::function<void(int)> onSelect);

    BRLS_BIND(Keyboard, m_keyboard, "search/keyboard");
    BRLS_BIND(brls::Label, m_hint, "search/hint");
    BRLS_BIND(brls::Box, m_results, "search/results");
    BRLS_BIND(ResultButtonGrid, m_buttonContainer, "search/buttonContainer");

    /** @brief XML 加载完成后初始化页面内容和操作 */
    void onContentAvailable() override;

private:
    std::vector<ResultButton*> m_resultButtons; // 当前搜索结果按钮
    brls::View* m_lastKeyboardFocus = nullptr;  // 键盘区域最后一个焦点
    std::vector<std::string> m_items;           // 搜索数据源
    std::function<void(int)> m_onSelect;        // 搜索结果选中回调
    SearchEngine m_searchEngine;                // 搜索引擎

    /**
     * @brief 显示搜索提示并隐藏结果
     * @param text 提示文字
     */
    void showHint(const std::string& text);

    /** @brief 判断当前焦点是否位于搜索结果区域 */
    bool isFocusInResults();

    /** @brief 将焦点切换到搜索结果区域 */
    void switchToResults();

    /** @brief 将焦点切换回虚拟键盘 */
    void switchToKeyboard();

    /**
     * @brief 根据关键词刷新搜索结果
     * @param keyword 当前搜索关键词
     */
    void updateResults(const std::string& keyword);

    /**
     * @brief 创建单个搜索结果按钮
     * @param result 搜索结果
     * @return 创建的搜索结果按钮
     */
    ResultButton* createResultButton(const SearchEngine::Result& result);
};
