/**
 * dontStarve - 饥荒特殊安装规则
 */

#pragma once

#include <bitset>
#include <string>

#include "core/modInstaller/dontStarveInstallMap.hpp"

namespace ModInstaller::dontStarve {

/**
 * @brief 饥荒安装前检查结果
 */
enum class InstallPrecheckResult {
    Ok,                // 检查通过，可以继续安装
    FrameworkMissing,  // 未检测到饥荒前置框架
};

/**
 * @brief 安装前检查饥荒前置框架
 * @param tid 当前游戏 TID
 */
InstallPrecheckResult checkBeforeInstall(const std::string& tid);

/**
 * @brief 饥荒本次安装路径规划器
 */
class DontStarveInstallPlanner {
public:
    /**
     * @brief 初始化本次安装规划状态
     * @param gameDirPath 当前游戏项目目录
     * @param modDirName 当前 MOD 目录名
     */
    bool prepare(const std::string& gameDirPath, const std::string& modDirName);

    /** @brief 处理单个文件目标路径 */
    void processFilePath(std::string& targetPath);

    /** @brief 处理单个目录目标路径 */
    void processDirectoryPath(std::string& targetPath);

    /** @brief 安装成功后保存本次饥荒映射记录 */
    bool save();

private:
    void processTargetPath(std::string& targetPath, bool isDirectory);

    DontStarveInstallMap m_installMap;       // 饥荒目录映射记录
    std::bitset<10000> m_usedBmNumbers;      // BM0001～BM9999 临时占用表
    std::string m_modDirName;                // 当前 MOD 目录名
    int m_nextBmNumber = 1;                  // 下一个待检查的 BM 编号
    bool m_changed = false;                  // 本次是否产生饥荒目录映射
};

/**
 * @brief 饥荒本次卸载路径规划器
 */
class DontStarveUninstallPlanner {
public:
    /**
     * @brief 初始化本次卸载规划状态
     * @param gameDirPath 当前游戏项目目录
     * @param modDirName 当前 MOD 目录名
     */
    bool prepare(const std::string& gameDirPath, const std::string& modDirName);

    /** @brief 处理单个文件目标路径 */
    bool processFilePath(std::string& targetPath);

    /** @brief 处理单个目录目标路径 */
    void processDirectoryPath(std::string& targetPath);

    /** @brief 卸载成功后删除当前 MOD 的饥荒映射记录 */
    bool save();

private:
    bool processTargetPath(std::string& targetPath, bool isDirectory);

    DontStarveInstallMap m_installMap;  // 饥荒目录映射记录
    std::string m_modDirName;           // 当前 MOD 目录名
    bool m_hasMappings = false;         // 当前 MOD 是否存在饥荒目录映射
};

} // namespace ModInstaller::dontStarve
