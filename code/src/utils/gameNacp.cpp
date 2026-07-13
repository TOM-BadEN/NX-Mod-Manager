/**
 * gameNacp - 游戏 NACP 数据获取工具
 * 回退链：libnxtc 缓存 → libnx ns 服务
 */

#include "utils/gameNacp.hpp"
#include "utils/format.hpp"
#include <switch.h>
#include <cstring>
#include <memory>
#include <zlib.h>

extern "C" {
#include <nxtc.h>
}

static constexpr size_t NACP_LANG_BLOCK_SIZE = 0x3000;
static constexpr size_t NACP_DECOMPRESS_BUF_SIZE = 0x6000;
static constexpr size_t NACP_COMPRESSION_OFFSET = 0x3215;
static constexpr uint8_t NACP_COMPRESSED_FLAG = 0x01;

/**
 * HOS 21.0.0+ 引入了压缩 NACP 标题块：nacp.lang[16] 区域不再直接存放
 * NacpLanguageEntry，而是 [u16 压缩长度 | zlib raw deflate 数据]。
 * nsGetApplicationControlData 返回的是未解压的原始数据，直接读 lang[i].name 会
 * 拿到二进制垃圾并导致越界崩溃。此函数检测压缩标志（偏移 0x3215），如果已压缩则
 * 用 zlib 解压后写回 lang 区域，使后续代码可以正常访问游戏名称。
 *
 * 解压算法参考：
 * - DarkMatterCore/libnxtc: nxtcDecompressNacpTitleBlock()
 * - masagrator/FPSLocker:   nacp_decompress()
 */
static void decompressNacpLang(NsApplicationControlData* controlData) {
    auto* raw = reinterpret_cast<uint8_t*>(&controlData->nacp);
    if (raw[NACP_COMPRESSION_OFFSET] != NACP_COMPRESSED_FLAG) return;

    uint16_t compressedSize = 0;
    std::memcpy(&compressedSize, raw, sizeof(uint16_t));
    const uint8_t* compressedData = raw + sizeof(uint16_t);

    auto buf = std::make_unique<uint8_t[]>(NACP_DECOMPRESS_BUF_SIZE);
    z_stream strm{};
    if (inflateInit2(&strm, -15) != Z_OK) return;
    strm.next_in = const_cast<Bytef*>(compressedData);
    strm.avail_in = compressedSize;
    strm.next_out = buf.get();
    strm.avail_out = NACP_DECOMPRESS_BUF_SIZE;
    int ret = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    if (ret == Z_STREAM_END) {
        std::memcpy(raw, buf.get(), NACP_LANG_BLOCK_SIZE);
    }
}

static uint32_t getGameVersion(uint64_t appId) {
    NsApplicationContentMetaStatus entries[16]{};
    s32 count = 0;
    uint32_t version = 0xFFFFFFFE;

    Result rc = nsListApplicationContentMetaStatus(appId, 0, entries, 16, &count);
    if (R_SUCCEEDED(rc)) {
        for (s32 i = 0; i < count && i < 16; i++) {
            if (entries[i].meta_type == NcmContentMetaType_Patch) {
                version = entries[i].version;
                break;
            }
        }
    }

    return version;
}

namespace gameNacp {

void init() {
    nsInitialize();
    nxtcInitialize();
}

void cleanup() {
    nxtcFlushCacheFile();
    nxtcExit();
    nsExit();
}

GameMetadata getGameNACP(uint64_t appId) {
    GameMetadata meta;

    uint32_t liveVersion = getGameVersion(appId);

    // 查缓存 + 比对版本
    NxTitleCacheApplicationMetadata* cached = nxtcGetApplicationMetadataEntryById(appId);
    if (cached && liveVersion == cached->version_info) {
        if (cached->name) meta.name = cached->name;
        if (cached->version) meta.version = cached->version;
        if (cached->icon_data && cached->icon_size > 0) {
            auto* data = static_cast<uint8_t*>(cached->icon_data);
            meta.icon.assign(data, data + cached->icon_size);
        }
        nxtcFreeApplicationMetadata(&cached);
        return meta;
    }
    if (cached) nxtcFreeApplicationMetadata(&cached);

    // 缓存未命中，调 ns 服务
    auto controlData = std::make_unique<NsApplicationControlData>();
    uint64_t jpegSize = 0;

    Result rc;
    if (hosversionAtLeast(20, 0, 0)) rc = nsGetApplicationControlData2(NsApplicationControlSource_Storage, appId, controlData.get(), sizeof(NsApplicationControlData), 0xFF, 0, &jpegSize, nullptr);
    else rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, appId, controlData.get(), sizeof(NsApplicationControlData), &jpegSize);
    if (R_FAILED(rc)) return meta;

    decompressNacpLang(controlData.get());

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
        nxtcAddEntry(appId, &controlData->nacp, iconSize, controlData->icon, true, liveVersion);
    }

    return meta;
}

std::string getEnglishName(uint64_t appId) {
    auto controlData = std::make_unique<NsApplicationControlData>();
    uint64_t jpegSize = 0;

    Result rc;
    if (hosversionAtLeast(20, 0, 0)) rc = nsGetApplicationControlData2(NsApplicationControlSource_Storage, appId, controlData.get(), sizeof(NsApplicationControlData), 0xFF, 0, &jpegSize, nullptr);
    else rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, appId, controlData.get(), sizeof(NsApplicationControlData), &jpegSize);

    std::string name;
    if (R_SUCCEEDED(rc)) {
        decompressNacpLang(controlData.get());
        const char* american = controlData->nacp.lang[0].name;  // AmericanEnglish
        const char* british = controlData->nacp.lang[1].name;  // BritishEnglish
        if (american[0] != '\0') name = american;
        else if (british[0] != '\0') name = british;
    }

    return name;
}

std::string getVersion(uint64_t appId) {
    uint32_t liveVersion = getGameVersion(appId);
    NxTitleCacheApplicationMetadata* cached = nxtcGetApplicationMetadataEntryById(appId);
    if (cached && cached->version && liveVersion == cached->version_info) {
        std::string version = cached->version;
        nxtcFreeApplicationMetadata(&cached);
        return version;
    }
    if (cached) nxtcFreeApplicationMetadata(&cached);

    auto controlData = std::make_unique<NsApplicationControlData>();
    uint64_t jpegSize = 0;
    Result rc;
    if (hosversionAtLeast(20, 0, 0)) rc = nsGetApplicationControlData2(NsApplicationControlSource_Storage, appId, controlData.get(), sizeof(NsApplicationControlData), 0xFF, 0, &jpegSize, nullptr);
    else rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, appId, controlData.get(), sizeof(NsApplicationControlData), &jpegSize);
    std::string version;
    if (R_FAILED(rc)) return version;

    version = controlData->nacp.display_version;

    if (jpegSize > sizeof(NacpStruct)) {
        size_t iconSize = jpegSize - sizeof(NacpStruct);
        nxtcAddEntry(appId, &controlData->nacp, iconSize, controlData->icon, true, liveVersion);
    }

    return version;
}

std::vector<uint64_t> getInstalledGameTids() {
    std::vector<uint64_t> tids;

    constexpr s32 PAGE_SIZE = 30;
    NsApplicationRecord records[PAGE_SIZE];
    s32 entryCount = 0;
    s32 offset = 0;

    while (true) {
        Result rc = nsListApplicationRecord(records, PAGE_SIZE, offset, &entryCount);
        if (R_FAILED(rc) || entryCount <= 0) break;

        entryCount = std::min(entryCount, PAGE_SIZE);

        for (s32 i = 0; i < entryCount; i++) {
            uint64_t appId = records[i].application_id;
            bool isBaseGame = format::appIdIsValid(appId);
            if (isBaseGame) tids.push_back(appId);
        }

        offset += entryCount;
    }

    return tids;
}

}
