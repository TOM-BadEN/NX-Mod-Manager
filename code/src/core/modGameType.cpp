/**
 * modGameType - Mod 适配视角下的游戏类型识别实现
 */

#include "core/modGameType.hpp"
#include "utils/format.hpp"

namespace {

struct ModGameTypeRule {
    const char* tid;   // 游戏 TID
    ModGameType type;  // 对应的 Mod 游戏类型
};

const ModGameTypeRule rules[] = {
    {"0100559011740000", ModGameType::MHRise},
    {"0100B04011742000", ModGameType::MHRise},
    {"0100751007ADA000", ModGameType::DontStarve},
};

} // namespace

namespace modGameType {

ModGameType detect(uint64_t appId) {
    std::string tid = format::appIdHex(appId);

    for (const auto& rule : rules) {
        if (tid == rule.tid) return rule.type;
    }

    return ModGameType::Normal;
}

} // namespace modGameType
