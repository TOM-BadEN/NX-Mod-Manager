/**
 * Help - 使用说明页面
 *
 * 左侧显示帮助条目列表，右侧显示当前条目的说明和二维码。
 * 页面通过 ShellState 向全局外壳提供标题。
 */

#pragma once

#include "ui/core/page.hpp"
#include "ui/core/shellState.hpp"
#include "ui/view/recyclingGrid.hpp"
#include <borealis.hpp>
#include <string>
#include <vector>

/** @brief 帮助条目中的一段文字 */
struct HelpText {
    std::string title;   // 段标题
    std::string content; // 段正文
};

/** @brief 帮助条目中的一个二维码 */
struct QrItem {
    std::string content; // 二维码内容
    std::string label;   // 二维码底部说明文字
};

/** @brief 单个帮助条目的完整内容 */
struct HelpEntry {
    std::string title;           // 左侧卡片标题
    std::vector<HelpText> texts; // 多段文字内容
    std::vector<QrItem> qrItems; // 二维码列表，空列表表示不显示二维码

    /**
     * @brief 创建帮助条目
     * @param t 左侧卡片标题
     */
    HelpEntry(const std::string& t) : title(t) {}

    /**
     * @brief 添加一段文字
     * @param title 段标题
     * @param content 段正文
     * @return 当前帮助条目
     */
    HelpEntry& addText(const std::string& title, const std::string& content = "") {
        texts.push_back({title, content});
        return *this;
    }

    /**
     * @brief 添加一个二维码
     * @param content 二维码内容
     * @param label 二维码底部说明文字
     * @return 当前帮助条目
     */
    HelpEntry& addQr(const std::string& content, const std::string& label = "") {
        qrItems.push_back({content, label});
        return *this;
    }
};

class Help : public Page, public ShellState {
public:
    /** @brief 创建使用说明页面并加载 XML 布局 */
    Help();

    BRLS_BIND(RecyclingGrid, m_grid, "help/grid");
    BRLS_BIND(brls::Box, m_detail, "help/detail");
    BRLS_BIND(brls::ScrollingFrame, m_scroll, "help/scroll");
    BRLS_BIND(brls::Box, m_container, "help/container");

    /** @brief XML 加载完成后初始化条目列表和焦点操作 */
    void onContentAvailable() override;

private:
    std::vector<HelpEntry> m_entries;       // 帮助条目数据
    std::vector<std::string> m_titles;      // 左侧卡片标题列表
    std::vector<brls::Box*> m_entryPanels;  // 非持有，面板由 m_container 统一释放
    brls::Box* m_visiblePanel = nullptr;    // 当前显示的详情面板
    size_t m_lastFocusIndex = 0;            // 左侧列表最后聚焦的条目索引

    /** @brief 填充帮助条目数据 */
    void buildEntries();

    /**
     * @brief 添加一个帮助条目
     * @param title 左侧卡片标题
     * @return 新添加的帮助条目
     */
    HelpEntry& addEntry(const std::string& title);

    /**
     * @brief 创建单个帮助条目的详情面板
     * @param entry 帮助条目数据
     * @return 新创建的详情面板
     */
    brls::Box* createEntryPanel(const HelpEntry& entry);

    /**
     * @brief 显示指定条目的内容
     * @param index 条目索引
     */
    void showEntry(size_t index);
};
