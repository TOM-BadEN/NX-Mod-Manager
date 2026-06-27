/**
 * ModInstaller - 目录模组安装与卸载
 */

#pragma once

#include "core/modInstaller/install.hpp"

namespace ModInstaller {

/**
 * @brief 从目录安装模组
 * @param mod 模组信息
 * @param game 游戏信息
 * @param modGameType 当前游戏的 MOD 适配类型
 * @param allMods 所有模组列表（用于冲突检测）
 * @param progressCb 进度回调
 * @param token 取消令牌
 */
InstallResult installFromDir(const ModInfo& mod, const GameInfo& game,
    ModGameType modGameType, const std::vector<ModInfo>& allMods,
    std::function<void(const Progress&)> progressCb, std::stop_token token);

/**
 * @brief 卸载目录模组
 * @param mod 模组信息
 * @param game 游戏信息
 * @param modGameType 当前游戏的 MOD 适配类型
 * @param progressCb 进度回调
 */
UninstallResult uninstallDir(const ModInfo& mod, const GameInfo& game,
    ModGameType modGameType, std::function<void(const Progress&)> progressCb);

} // namespace ModInstaller
