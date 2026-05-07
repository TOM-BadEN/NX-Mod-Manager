/**
 * SearchActivity - 搜索页面
 * 独立 Activity，内置虚拟键盘 + 搜索结果列表
 */

#pragma once

#include <borealis.hpp>
#include <functional>
#include <vector>
#include <string>
#include "view/myframe.hpp"
#include "view/keyboard.hpp"
#include "utils/searchEngine.hpp"

/** @brief 搜索结果按钮（抑制焦点切换时的动画残留） */
class ResultButton : public brls::Box {
public:
    void onFocusGained() override;
    void onFocusLost() override;
};

/** @brief 搜索结果按钮网格容器，统一处理导航和音效 */
class ResultButtonGrid : public brls::Box {
public:
    /**
     * @brief 设置按钮列表指针
     * @param buttons 结果按钮列表
     */
    void setButtons(std::vector<ResultButton*>* buttons) { m_buttons = buttons; }

    /**
     * @brief 获取下一个焦点
     * @param direction 导航方向
     * @param currentView 当前焦点视图
     */
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    static brls::View* create();

private:
    std::vector<ResultButton*>* m_buttons = nullptr;
};

class SearchActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/searchActivity.xml");

    /**
     * @brief 构造搜索页面
     * @param items 搜索数据源
     * @param onSelect 选中回调
     */
    SearchActivity(const std::vector<std::string>& items, std::function<void(int)> onSelect);

    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

private:
    BRLS_BIND(MyFrame, m_frame, "search/frame");                           // 主框架
    BRLS_BIND(Keyboard, m_keyboard, "search/keyboard");                    // 虚拟键盘
    BRLS_BIND(brls::Label, m_hint, "search/hint");                         // 提示文本
    BRLS_BIND(brls::Box, m_results, "search/results");                     // 搜索结果区域
    BRLS_BIND(ResultButtonGrid, m_buttonContainer, "search/buttonContainer");  // 结果按钮容器

    // RT 切换区域
    std::vector<ResultButton*> m_resultButtons;                            // 当前结果按钮列表
    brls::View* m_lastKeyboardFocus = nullptr;                             // 键盘区最后焦点（RT 切回时恢复）

    std::vector<std::string> m_items;                                      // 搜索数据源
    std::function<void(int)> m_onSelect;                                   // 选中回调
    SearchEngine m_searchEngine;                                           // 搜索引擎

    /**
     * @brief 显示提示文本，隐藏结果，禁用 RT
     * @param text 提示内容
     */
    void showHint(const std::string& text);

    /** @brief 判断焦点是否在结果区 */
    bool isFocusInResults();

    /**
     * @brief 根据关键词更新结果
     * @param keyword 搜索关键词
     */
    void updateResults(const std::string& keyword);

    /** @brief 焦点切换到结果区 */
    void switchToResults();

    /** @brief 焦点切换回键盘 */
    void switchToKeyboard();

    /**
     * @brief 创建单个结果按钮
     * @param result 搜索结果条目
     */
    ResultButton* createResultButton(const SearchEngine::Result& result);
};
