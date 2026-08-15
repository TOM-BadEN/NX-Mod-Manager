/**
 * VersionHistory - 历史版本页面
 *
 * 左侧显示版本号列表，右侧显示当前版本的发布时间和更新日志。
 * 页面通过 ShellState 向全局外壳提供标题。
 */

#pragma once

#include "api/app.hpp"
#include "ui/core/page.hpp"
#include "ui/core/shellState.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "ui/view/scrollHint.hpp"
#include <borealis.hpp>
#include <string>
#include <vector>

class VersionHistory : public Page, public ShellState {
public:
    /**
     * @brief 创建历史版本页面并加载 XML 布局
     * @param versions 历史版本列表
     */
    explicit VersionHistory(std::vector<api::app::VersionHistoryItem> versions);

    BRLS_BIND(RecyclingGrid, m_grid, "versionHistory/grid");
    BRLS_BIND(brls::Box, m_detail, "versionHistory/detail");
    BRLS_BIND(brls::ScrollingFrame, m_scroll, "versionHistory/scroll");
    BRLS_BIND(brls::Box, m_scrollContent, "versionHistory/scrollContent");
    BRLS_BIND(ScrollHint, m_scrollHint, "versionHistory/scrollHint");
    BRLS_BIND(brls::Label, m_versionTitle, "versionHistory/versionTitle");
    BRLS_BIND(brls::Label, m_publishTime, "versionHistory/publishTime");
    BRLS_BIND(brls::Label, m_releaseNotes, "versionHistory/releaseNotes");

    /** @brief XML 加载完成后初始化版本列表和焦点操作 */
    void onContentAvailable() override;

    /** @brief 布局更新后刷新底部滚动提示 */
    void onLayout() override;

private:
    std::vector<api::app::VersionHistoryItem> m_versions; // 历史版本数据
    std::vector<std::string> m_versionTags;               // 左侧卡片版本号列表
    size_t m_lastFocusIndex = 0;                          // 左侧列表最后聚焦的版本索引
    bool m_layoutReady = false;                           // 页面内容是否已经完成初始化

    /** @brief 设置页面标题 */
    void setHeader();

    /** @brief 初始化版本列表 */
    void setupGrid();

    /** @brief 初始化详情面板操作 */
    void setupDetail();

    /** @brief 根据焦点和内容高度更新底部滚动提示 */
    void updateScrollHintVisibility();

    /**
     * @brief 显示指定版本的内容
     * @param index 版本索引
     */
    void showVersion(size_t index);

    /**
     * @brief 获取当前语言对应的更新日志
     * @param version 历史版本数据
     * @return 当前语言对应的更新日志
     */
    const std::string& releaseNotes(const api::app::VersionHistoryItem& version) const;
};
