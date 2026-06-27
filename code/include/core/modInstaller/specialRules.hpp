/**
 * specialRules - 安装特殊规则分发
 */

#pragma once

#include <memory>
#include <string>

#include "common/gameInfo.hpp"
#include "common/modInfo.hpp"
#include "core/modGameType.hpp"
#include "core/modInstaller/mhrise.hpp"

namespace ModInstaller {

/**
 * @brief 安装特殊规则分发器
 */
class SpecialModInstallRules {
public:
    /**
     * @brief 初始化安装特殊规则状态
     */
    bool init(ModGameType modGameType, const ModInfo& mod, const GameInfo& game);

    /**
     * @brief 安装单个文件前应用特殊规则
     */
    bool apply(std::string& targetPath);

    /**
     * @brief 安装成功后保存特殊规则记录
     */
    bool save();

private:
    ModGameType m_modGameType = ModGameType::Normal;                           // 当前游戏的 MOD 适配类型
    std::unique_ptr<mhrise::MHRiseInstallPlanner> m_mhriseInstallPlanner;       // 怪猎安装规则对象
};

/**
 * @brief 卸载特殊规则分发器
 */
class SpecialModUninstallRules {
public:
    /**
     * @brief 初始化卸载特殊规则状态
     */
    bool init(ModGameType modGameType, const ModInfo& mod, const GameInfo& game, const std::string& tid);

    /**
     * @brief 卸载单个文件前应用特殊规则
     */
    bool apply(std::string& targetPath);

    /**
     * @brief 卸载成功后保存特殊规则记录
     */
    bool save();

private:
    ModGameType m_modGameType = ModGameType::Normal;                               // 当前游戏的 MOD 适配类型
    std::unique_ptr<mhrise::MHRiseUninstallPlanner> m_mhriseUninstallPlanner;       // 怪猎卸载规则对象
};

} // namespace ModInstaller
