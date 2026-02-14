/**
 * GameNACP - 游戏数据获取封装
 * 回退链：libnxtc 缓存 → libnx ns 服务
 */

#include "utils/gameNACP.hpp"
#include <switch.h>
#include <cstring>
#include <memory>

extern "C" {
#include <nxtc_version.h>
}

GameNACP::GameNACP() {
    nsInitialize();
    nxtcInitialize();
}

GameNACP::~GameNACP() {
    nxtcFlushCacheFile();
    nxtcExit();
    nsExit();
}

GameMetadata GameNACP::getGameNACP(uint64_t appId) {
    GameMetadata meta;

    // 先查 libnxtc 缓存
    NxTitleCacheApplicationMetadata* cached = nxtcGetApplicationMetadataEntryById(appId);
    if (cached) {
        if (cached->name) meta.name = cached->name;
        if (cached->version) meta.version = cached->version;
        if (cached->icon_data && cached->icon_size > 0) {
            auto* data = static_cast<uint8_t*>(cached->icon_data);
            meta.icon.assign(data, data + cached->icon_size);
        }
        nxtcFreeApplicationMetadata(&cached);
        return meta;
    }

    // 缓存未命中，调 ns 服务
    auto controlData = std::make_unique<NsApplicationControlData>();
    uint64_t jpegSize = 0;

    Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, appId, controlData.get(), sizeof(NsApplicationControlData), &jpegSize);
    if (R_FAILED(rc)) return meta;

    // 取游戏名（按系统语言）
    NacpLanguageEntry* langEntry = nullptr;
    rc = nacpGetLanguageEntry(&controlData->nacp, &langEntry);
    if (R_SUCCEEDED(rc) && langEntry && langEntry->name[0] != '\0') {
        meta.name = langEntry->name;
    }

    // 取版本号
    meta.version = controlData->nacp.display_version;

    // 取图标 JPEG 数据并写入 libnxtc 缓存
    if (jpegSize > sizeof(NacpStruct)) {
        size_t iconSize = jpegSize - sizeof(NacpStruct);
        meta.icon.assign(controlData->icon, controlData->icon + iconSize);
        nxtcAddEntry(appId, &controlData->nacp, iconSize, controlData->icon, false, 0);
    }

    return meta;
}
