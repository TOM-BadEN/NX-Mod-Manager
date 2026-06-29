/**
 * specialRules - 安装特殊规则分发实现
 */

#include "core/modInstaller/specialRules.hpp"

namespace ModInstaller {

bool SpecialModInstallRules::init(ModGameType modGameType, const ModInfo& mod, const GameInfo& game) {
    m_modGameType = modGameType;
    m_mhriseInstallPlanner.reset();

    switch (m_modGameType) {
        case ModGameType::MHRise:
            m_mhriseInstallPlanner = std::make_unique<mhrise::MHRiseInstallPlanner>();
            return m_mhriseInstallPlanner->prepare(game.dirPath, game.version, mod.dirName);

        case ModGameType::Normal:
        default:
            return true;
    }
}

bool SpecialModInstallRules::apply(std::string& targetPath) {
    switch (m_modGameType) {
        case ModGameType::MHRise:
            if (!m_mhriseInstallPlanner) return false;
            return m_mhriseInstallPlanner->processTargetPath(targetPath);

        case ModGameType::Normal:
        default:
            return true;
    }
}

bool SpecialModInstallRules::save() {
    switch (m_modGameType) {
        case ModGameType::MHRise:
            if (!m_mhriseInstallPlanner) return false;
            return m_mhriseInstallPlanner->save();

        case ModGameType::Normal:
        default:
            return true;
    }
}

bool SpecialModUninstallRules::init(ModGameType modGameType, const ModInfo& mod, const GameInfo& game, const std::string& tid) {
    m_modGameType = modGameType;
    m_mhriseUninstallPlanner.reset();

    switch (m_modGameType) {
        case ModGameType::MHRise:
            m_mhriseUninstallPlanner = std::make_unique<mhrise::MHRiseUninstallPlanner>();
            return m_mhriseUninstallPlanner->prepare(game.dirPath, tid, mod.dirName);

        case ModGameType::Normal:
        default:
            return true;
    }
}

bool SpecialModUninstallRules::apply(std::string& targetPath) {
    switch (m_modGameType) {
        case ModGameType::MHRise:
            if (!m_mhriseUninstallPlanner) return false;
            return m_mhriseUninstallPlanner->processTargetPath(targetPath);

        case ModGameType::Normal:
        default:
            return true;
    }
}

bool SpecialModUninstallRules::save() {
    switch (m_modGameType) {
        case ModGameType::MHRise:
            if (!m_mhriseUninstallPlanner) return false;
            return m_mhriseUninstallPlanner->save();

        case ModGameType::Normal:
        default:
            return true;
    }
}

} // namespace ModInstaller
