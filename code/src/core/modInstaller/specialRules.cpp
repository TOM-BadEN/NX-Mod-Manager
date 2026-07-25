/**
 * specialRules - 安装特殊规则分发实现
 */

#include "core/modInstaller/specialRules.hpp"

namespace ModInstaller {

bool SpecialModInstallRules::init(ModGameType modGameType, const ModInfo& mod, const GameInfo& game) {
    m_modGameType = modGameType;

    switch (m_modGameType) {
        case ModGameType::MHRise:
            m_mhriseInstallPlanner = std::make_unique<mhrise::MHRiseInstallPlanner>();
            return m_mhriseInstallPlanner->prepare(game.dirPath, game.version, mod.dirName);

        case ModGameType::DontStarve:
            m_dontStarveInstallPlanner = std::make_unique<dontStarve::DontStarveInstallPlanner>();
            return m_dontStarveInstallPlanner->prepare(game.dirPath, mod.dirName);

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

        case ModGameType::DontStarve:
            if (!m_dontStarveInstallPlanner) return false;
            m_dontStarveInstallPlanner->processFilePath(targetPath);
            return true;

        case ModGameType::Normal:
        default:
            return true;
    }
}

void SpecialModInstallRules::applyDirectory(std::string& targetPath) {
    switch (m_modGameType) {
        case ModGameType::DontStarve:
            if (!m_dontStarveInstallPlanner) return;
            m_dontStarveInstallPlanner->processDirectoryPath(targetPath);
            return;

        case ModGameType::MHRise:
        case ModGameType::Normal:
        default:
            return;
    }
}

bool SpecialModInstallRules::save() {
    switch (m_modGameType) {
        case ModGameType::MHRise:
            if (!m_mhriseInstallPlanner) return false;
            return m_mhriseInstallPlanner->save();

        case ModGameType::DontStarve:
            if (!m_dontStarveInstallPlanner) return false;
            return m_dontStarveInstallPlanner->save();

        case ModGameType::Normal:
        default:
            return true;
    }
}

bool SpecialModUninstallRules::init(ModGameType modGameType, const ModInfo& mod, const GameInfo& game, const std::string& tid) {
    m_modGameType = modGameType;

    switch (m_modGameType) {
        case ModGameType::MHRise:
            m_mhriseUninstallPlanner = std::make_unique<mhrise::MHRiseUninstallPlanner>();
            return m_mhriseUninstallPlanner->prepare(game.dirPath, tid, mod.dirName);

        case ModGameType::DontStarve:
            m_dontStarveUninstallPlanner = std::make_unique<dontStarve::DontStarveUninstallPlanner>();
            return m_dontStarveUninstallPlanner->prepare(game.dirPath, mod.dirName);

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

        case ModGameType::DontStarve:
            if (!m_dontStarveUninstallPlanner) return false;
            return m_dontStarveUninstallPlanner->processFilePath(targetPath);

        case ModGameType::Normal:
        default:
            return true;
    }
}

void SpecialModUninstallRules::applyDirectory(std::string& targetPath) {
    switch (m_modGameType) {
        case ModGameType::DontStarve:
            if (!m_dontStarveUninstallPlanner) return;
            m_dontStarveUninstallPlanner->processDirectoryPath(targetPath);
            return;

        case ModGameType::MHRise:
        case ModGameType::Normal:
        default:
            return;
    }
}

bool SpecialModUninstallRules::save() {
    switch (m_modGameType) {
        case ModGameType::MHRise:
            if (!m_mhriseUninstallPlanner) return false;
            return m_mhriseUninstallPlanner->save();

        case ModGameType::DontStarve:
            if (!m_dontStarveUninstallPlanner) return false;
            return m_dontStarveUninstallPlanner->save();

        case ModGameType::Normal:
        default:
            return true;
    }
}

} // namespace ModInstaller
