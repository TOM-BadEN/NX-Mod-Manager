/**
 * mhrise - 怪物猎人 崛起特殊安装规则
 */

#pragma once

#include <cstdint>
#include <string>

#include "core/modInstaller/mhriseInstallMap.hpp"

namespace ModInstaller::mhrise {

/**
 * @brief 怪猎安装前检查结果
 */
enum class InstallPrecheckResult {
    Ok,                    // 检查通过，可以继续安装
    GameNotInstalled,      // 主机未安装怪物猎人 崛起本体
    VersionUnsupported,    // 无法读取当前游戏版本，或当前版本不在支持映射表中
    VersionChanged,        // 已安装记录的游戏版本与当前版本不一致
};

/**
 * @brief 安装前检查怪猎特殊规则
 * @param currentVersion 当前游戏版本
 * @param gameInstalled 主机是否已安装该游戏本体
 * @param gameDirPath 当前游戏项目目录
 */
InstallPrecheckResult checkBeforeInstall(const std::string& currentVersion,
    bool gameInstalled, const std::string& gameDirPath);

/**
 * @brief 怪猎本次安装路径规划器
 */
class MHRiseInstallPlanner {
public:
    /**
     * @brief 初始化本次安装规划状态
     * @param gameDirPath 当前游戏项目目录
     * @param currentVersion 当前游戏版本
     * @param modDirName 当前 MOD 目录名
     */
    bool prepare(const std::string& gameDirPath, const std::string& currentVersion, const std::string& modDirName);

    /**
     * @brief 处理单个目标路径，特殊 pak 会直接修改 targetPath
     * @param targetPath 标准流程生成的目标路径
     */
    bool processTargetPath(std::string& targetPath);

    /** @brief 安装成功后保存本次怪猎映射记录 */
    bool save();

private:
    MHRiseInstallMap m_installMap;   // 怪猎特殊 pak 映射记录
    std::string m_modDirName;        // 当前 MOD 目录名
    std::string m_gameVersion;       // 格式化后的游戏版本
    int m_patchNo = 0;               // 本次准备分配的 patch 编号
    int m_lastPatchNo = 0;           // 最后已记录的 patch 编号
    bool m_ready = false;            // 是否已完成初始化
    bool m_changed = false;          // 本次是否产生特殊 pak 映射
};

/**
 * @brief 怪猎本次卸载路径规划器
 */
class MHRiseUninstallPlanner {
public:
    /**
     * @brief 初始化本次卸载规划状态
     * @param gameDirPath 当前游戏项目目录
     * @param tid 当前游戏 TID
     * @param modDirName 当前 MOD 目录名
     */
    bool prepare(const std::string& gameDirPath, const std::string& tid, const std::string& modDirName);

    /**
     * @brief 处理单个目标路径，特殊 pak 会直接改成实际安装文件名
     * @param targetPath 标准流程生成的目标路径
     */
    bool processTargetPath(std::string& targetPath);

    /** @brief 卸载成功后修正后续编号并保存映射记录 */
    bool save();

private:
    MHRiseInstallMap m_installMap;                 // 怪猎特殊 pak 映射记录
    std::string m_modDirName;                      // 当前 MOD 目录名
    std::string m_romfsPath;                       // 当前游戏 romfs 目录路径
    int m_minRemovedPatchNo = 0;                   // 本次卸载释放的最小 patch 编号
    int m_removedCount = 0;                        // 本次卸载释放的 patch 数量
    bool m_changed = false;                        // 本次是否处理过特殊 pak
};

} // namespace ModInstaller::mhrise
