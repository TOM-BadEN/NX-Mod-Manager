/**
 * ModManager - Mod 管理页面
 * 左侧 ModList + 右侧 ModDetail，扫描并显示某个游戏的所有 mod
 */

#pragma once

#include <borealis.hpp>
#include <vector>
#include "view/myframe.hpp"
#include "view/modList.hpp"
#include "view/modDetail.hpp"
#include "common/modInfo.hpp"

class ModManager : public brls::Activity {
public:
    ModManager(const std::string& dirPath, const std::string& gameName);

    CONTENT_FROM_XML_RES("activity/modManager.xml");

    BRLS_BIND(MyFrame, m_frame, "modManager/frame");
    BRLS_BIND(ModList, m_modList, "modManager/modList");
    BRLS_BIND(ModDetail, m_modDetail, "modManager/modDetail");

    void onContentAvailable() override;

private:
    std::string m_dirPath;
    std::string m_gameName;
    std::vector<ModInfo> m_mods;
};