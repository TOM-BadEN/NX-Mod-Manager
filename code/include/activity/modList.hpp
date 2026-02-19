/**
 * ModList - Mod 列表页面
 * 左侧 RecyclingGrid（单列 Mod 列表） + 右侧 Mod 详情
 */

#pragma once

#include <borealis.hpp>
#include <vector>
#include <cstdint>
#include "view/myframe.hpp"
#include "view/recyclingGrid.hpp"
#include "common/modInfo.hpp"

class ModList : public brls::Activity {
public:
    ModList(const std::string& dirPath, const std::string& gameName, uint64_t appId);

    CONTENT_FROM_XML_RES("activity/modList.xml");

    BRLS_BIND(MyFrame, m_frame, "modList/frame");
    BRLS_BIND(RecyclingGrid, m_grid, "modList/grid");
    BRLS_BIND(brls::Box, m_detail, "modList/detail");
    BRLS_BIND(brls::ScrollingFrame, m_scroll, "modList/scroll");
    BRLS_BIND(brls::Box, m_tagRow, "modList/tagRow");

    // 详情面板绑定
    BRLS_BIND(brls::Image, m_gameIcon, "modList/gameIcon");
    BRLS_BIND(brls::Label, m_gameNameLabel, "modList/gameName");
    BRLS_BIND(brls::Label, m_gameTid, "modList/gameTid");
    BRLS_BIND(brls::Label, m_tagType, "modList/tagType");
    BRLS_BIND(brls::Label, m_tagVersion, "modList/tagVersion");
    BRLS_BIND(brls::Label, m_tagAuthor, "modList/tagAuthor");
    BRLS_BIND(brls::Label, m_tagGameVer, "modList/tagGameVer");
    BRLS_BIND(brls::Label, m_tagSize, "modList/tagSize");
    BRLS_BIND(brls::Label, m_tagFormat, "modList/tagFormat");
    BRLS_BIND(brls::Label, m_descBody, "modList/descBody");

    void onContentAvailable() override;

private:
    std::string m_dirPath;                  // 游戏 mod 目录路径
    std::string m_gameName;                 // 游戏名称
    uint64_t m_appId = 0;                   // 游戏 TID
    std::vector<ModInfo> m_mods;            // mod 列表数据
    bool m_sortAsc = true;                  // 当前排序方向
    size_t m_lastFocusIndex = 0;             // 切换到详情前记住的列表索引
    int m_iconRetryLeft = 10;                // 图标重试剩余次数（每秒1次，最多10次）

    void setupModGrid();                    // 初始化网格（注册 Cell、绑定数据源、设置回调）
    void setupDetail();                     // 初始化详情面板（游戏图标/名/TID）
    void updateDetail(size_t index);        // 根据焦点更新详情面板
    void toggleSort();                      // 切换排序方向
    void flipScreen(int direction);         // LB/RB 翻页
    void retryIconLoad();                   // 图标延迟加载重试（每秒1次，最多10次）
};