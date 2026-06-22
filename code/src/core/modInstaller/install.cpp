/**
 * ModInstaller - 公共 API 分发
 */

#include "core/modInstaller/install.hpp"
#include "core/modInstaller/installZip.hpp"
#include "core/modInstaller/installDir.hpp"

namespace ModInstaller {

InstallResult install(const ModInfo& mod, const GameInfo& game, ModGameType modGameType,
    const std::vector<ModInfo>& allMods, std::function<void(const Progress&)> progressCb,
    std::stop_token token) {

    if (mod.isZip) return installFromZip(mod, game, modGameType, allMods, progressCb, token);
    return installFromDir(mod, game, modGameType, allMods, progressCb, token);
}

UninstallResult uninstall(const ModInfo& mod, const GameInfo& game, ModGameType modGameType,
    std::function<void(const Progress&)> progressCb) {

    if (mod.isZip) return uninstallZip(mod, game, modGameType, progressCb);
    return uninstallDir(mod, game, modGameType, progressCb);
}

} // namespace ModInstaller
