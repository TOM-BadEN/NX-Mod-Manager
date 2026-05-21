/**
 * StoreModLocalState - 商店模组本地下载状态实现
 */

#include "core/storeModLocalState.hpp"
#include "common/config.hpp"
#include "utils/format.hpp"
#include "utils/jsonFile.hpp"
#include <cstdlib>

void StoreModLocalState::loadFromModManager(ModManager& modManager) {
    for (const auto& mod : modManager.mods()) {
        if (mod.modID > 0) m_downloadedModIds.insert(mod.modID);
    }
}

void StoreModLocalState::loadFromGame(GameManager& gameManager, const std::string& gameTid) {
    uint64_t appId = format::appIdFromHex(gameTid);
    int idx = gameManager.findByAppId(appId);
    if (idx < 0) return;

    const auto& game = gameManager.games()[idx];
    JsonFile modJson;
    modJson.load(game.dirPath + config::modInfoFile);

    for (const auto& rootKey : modJson.getRootKeys()) {
        std::string modIdStr = modJson.getString(rootKey, "modID");
        if (modIdStr.empty()) continue;

        int modId = std::atoi(modIdStr.c_str());
        if (modId > 0) m_downloadedModIds.insert(modId);
    }
}

bool StoreModLocalState::isDownloaded(int modId) const {
    return m_downloadedModIds.count(modId) > 0;
}

void StoreModLocalState::markDownloaded(int modId) {
    if (modId > 0) m_downloadedModIds.insert(modId);
}
