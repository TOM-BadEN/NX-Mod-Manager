/**
 * specialRules - 安装特殊规则分发
 */

#pragma once

#include <memory>
#include <string>

#include "common/gameInfo.hpp"
#include "common/modInfo.hpp"
#include "core/modGameType.hpp"
#include "core/modInstaller/dontStarve.hpp"
#include "core/modInstaller/mhrise.hpp"

namespace ModInstaller {

/**
 * @brief 安装特殊规则分发器
 */
class SpecialModInstallRules {
public:
    /**
     * @brief 初始化安装特殊规则状态
     * @param modGameType 当前游戏的 MOD 适配类型
     * @param mod 模组信息
     * @param game 游戏信息
     * @return 是否初始化成功
     */
    bool init(ModGameType modGameType, const ModInfo& mod, const GameInfo& game);

    /**
     * @brief 安装单个文件前应用特殊规则
     * @param targetPath 待处理的目标路径
     * @return 是否处理成功
     */
    bool apply(std::string& targetPath);

    /**
     * @brief 安装单个目录前应用特殊规则
     * @param targetPath 待处理的目标路径
     */
    void applyDirectory(std::string& targetPath);

    /**
     * @brief 安装成功后保存特殊规则记录
     * @return 是否保存成功
     */
    bool save();

private:
    ModGameType m_modGameType = ModGameType::Normal;                                   // 当前游戏的 MOD 适配类型
    std::unique_ptr<mhrise::MHRiseInstallPlanner> m_mhriseInstallPlanner;               // 怪猎安装规则对象
    std::unique_ptr<dontStarve::DontStarveInstallPlanner> m_dontStarveInstallPlanner;   // 饥荒安装规则对象
};

/**
 * @brief 卸载特殊规则分发器
 */
class SpecialModUninstallRules {
public:
    /**
     * @brief 初始化卸载特殊规则状态
     * @param modGameType 当前游戏的 MOD 适配类型
     * @param mod 模组信息
     * @param game 游戏信息
     * @param tid 当前游戏 TID
     * @return 是否初始化成功
     */
    bool init(ModGameType modGameType, const ModInfo& mod, const GameInfo& game, const std::string& tid);

    /**
     * @brief 卸载单个文件前应用特殊规则
     * @param targetPath 待处理的目标路径
     * @return 是否处理成功
     */
    bool apply(std::string& targetPath);

    /**
     * @brief 卸载单个目录前应用特殊规则
     * @param targetPath 待处理的目标路径
     */
    void applyDirectory(std::string& targetPath);

    /**
     * @brief 卸载成功后保存特殊规则记录
     * @return 是否保存成功
     */
    bool save();

private:
    ModGameType m_modGameType = ModGameType::Normal;                                       // 当前游戏的 MOD 适配类型
    std::unique_ptr<mhrise::MHRiseUninstallPlanner> m_mhriseUninstallPlanner;               // 怪猎卸载规则对象
    std::unique_ptr<dontStarve::DontStarveUninstallPlanner> m_dontStarveUninstallPlanner;   // 饥荒卸载规则对象
};

} // namespace ModInstaller
