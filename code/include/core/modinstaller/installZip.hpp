/**
 * ModInstaller - ZIP 模组安装与卸载
 */

#pragma once

#include "core/modInstaller/install.hpp"

namespace ModInstaller {

/**
 * @brief 从 ZIP 安装模组
 * @param mod 模组信息
 * @param game 游戏信息
 * @param allMods 所有模组列表（用于冲突检测）
 * @param progressCb 进度回调
 * @param token 取消令牌
 */
InstallResult installFromZip(const ModInfo& mod, const GameInfo& game,
    const std::vector<ModInfo>& allMods, std::function<void(const Progress&)> progressCb,
    std::stop_token token);

/**
 * @brief 卸载 ZIP 模组
 * @param mod 模组信息
 * @param game 游戏信息
 * @param progressCb 进度回调
 */
UninstallResult uninstallZip(const ModInfo& mod, const GameInfo& game,
    std::function<void(const Progress&)> progressCb);

} // namespace ModInstaller
