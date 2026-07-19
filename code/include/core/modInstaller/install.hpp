/**
 * ModInstaller - 公共类型、公共 API
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <stop_token>
#include "common/modInfo.hpp"
#include "common/gameInfo.hpp"
#include "core/modGameType.hpp"

namespace ModInstaller {

// ============================================================================
// 公共类型
// ============================================================================

struct Progress {
    bool cleaning = false;      // true 表示清理/回滚阶段
    int current;                // 当前已处理的文件序号（从 1 开始）
    int total;                  // 需要处理的文件总数
    std::string currentFile;    // 当前处理的文件路径
    int64_t bytesWritten;       // 当前文件已写入字节数
    int64_t bytesTotal;         // 当前文件总字节数
};

struct InstallResult {
    bool success = false;
    std::string errorFile;      // 失败时：出错的文件路径
    std::string errorMsg;
    std::string conflictMod;    // CRC 冲突时，对方 mod 名称
};

struct UninstallResult {
    bool success = false;
    std::string errorFile;      // 失败时：出错的文件路径
    std::string errorMsg;
};

// ============================================================================
// 公共 API
// ============================================================================

/**
 * @brief 安装模组（自动识别 ZIP/目录）
 * @param mod 模组信息
 * @param game 游戏信息
 * @param modGameType 当前游戏的 MOD 适配类型
 * @param allMods 所有模组列表（用于冲突检测）
 * @param progressCb 进度回调
 * @param token 取消令牌
 */
InstallResult install(const ModInfo& mod, const GameInfo& game, ModGameType modGameType,
    const std::vector<ModInfo>& allMods, std::function<void(const Progress&)> progressCb = nullptr,
    std::stop_token token = {});

/**
 * @brief 卸载模组（自动识别 ZIP/目录）
 * @param mod 模组信息
 * @param game 游戏信息
 * @param modGameType 当前游戏的 MOD 适配类型
 * @param progressCb 进度回调
 */
UninstallResult uninstall(const ModInfo& mod, const GameInfo& game, ModGameType modGameType,
    std::function<void(const Progress&)> progressCb = nullptr);

} // namespace ModInstaller
