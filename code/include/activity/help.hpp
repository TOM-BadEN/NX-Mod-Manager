/**
 * Help - 使用说明页面
 * 左侧 RecyclingGrid（说明条目标题） + 右侧动态内容
 */

#pragma once

#include <borealis.hpp>
#include <vector>
#include <string>
#include "view/myframe.hpp"
#include "view/recyclingGrid.hpp"

struct HelpText {
    std::string title;    // 大文本（段标题）
    std::string content;  // 小文本（段正文）
};

struct QrItem {
    std::string content;  // 二维码内容
    std::string label;    // 底部说明文字
};

struct HelpEntry {
    std::string title;                  // 左侧卡片标题
    std::vector<HelpText> texts;        // 多段内容
    std::vector<QrItem> qrItems;        // 二维码列表，空则不显示

    HelpEntry(const std::string& t) : title(t) {}

    HelpEntry& addText(const std::string& title, const std::string& content = "") {
        texts.push_back({title, content});
        return *this;
    }

    HelpEntry& addQr(const std::string& content, const std::string& label = "") {
        qrItems.push_back({content, label});
        return *this;
    }
};

class Help : public brls::Activity {
public:
    Help();

    CONTENT_FROM_XML_RES("activity/help.xml");

    BRLS_BIND(MyFrame, m_frame, "help/frame");
    BRLS_BIND(RecyclingGrid, m_grid, "help/grid");
    BRLS_BIND(brls::Box, m_detail, "help/detail");
    BRLS_BIND(brls::ScrollingFrame, m_scroll, "help/scroll");
    BRLS_BIND(brls::Box, m_container, "help/container");

    /** @brief XML 加载完成后调用 */
    void onContentAvailable() override;

private:
    std::vector<HelpEntry> m_entries;
    std::vector<std::string> m_titles;
    size_t m_lastFocusIndex = 0;

    /** @brief 填充条目数据 */
    void buildEntries();

    /** @brief 添加一个条目并返回引用 */
    HelpEntry& addEntry(const std::string& title);

    /**
     * @brief 显示指定条目的内容
     * @param index 条目索引
     */
    void showEntry(size_t index);
};
