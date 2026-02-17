/**
 * ModList - Mod 列表页面
 * 左侧 RecyclingGrid（单列 Mod 列表） + 右侧 Mod 详情
 */

#pragma once

#include <borealis.hpp>
#include <vector>
#include "view/myframe.hpp"
#include "view/recyclingGrid.hpp"
#include "common/modInfo.hpp"

class ModList : public brls::Activity {
public:
    ModList(const std::string& dirPath, const std::string& gameName);

    CONTENT_FROM_XML_RES("activity/modList.xml");

    BRLS_BIND(MyFrame, m_frame, "modList/frame");
    BRLS_BIND(RecyclingGrid, m_grid, "modList/grid");
    BRLS_BIND(brls::Box, m_detail, "modList/detail");

    void onContentAvailable() override;

private:
    std::string m_dirPath;                  // 游戏 mod 目录路径
    std::string m_gameName;                 // 游戏名称
    std::vector<ModInfo> m_mods;            // mod 列表数据
    bool m_sortAsc = true;                  // 当前排序方向

    void setupModGrid();                    // 初始化网格（注册 Cell、绑定数据源、设置回调）
    void toggleSort();                      // 切换排序方向
    void flipScreen(int direction);            // LB/RB 翻页
};