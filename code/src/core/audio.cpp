/**
 * Audio - 音效模块
 * 使用 switch-libpulsar 从 qlaunch.bfsar 加载并播放 UI 音效
 */

#include "core/audio.hpp"
#include <switch.h>

struct SoundMapping {
    SoundEffect effect;
    const char* name;
};

constexpr SoundMapping SOUND_MAP[] = {
    { SoundEffect::Focus,      "SeGameIconFocus" },
    { SoundEffect::FocusLimit, "SeGameIconLimit" },
    { SoundEffect::Click,      "SeBtnDecide" },
    { SoundEffect::Warning,    "SeInsertError" },
    { SoundEffect::Enter,      "SeGameIconAdd" },
};

Audio::Audio() {
    s_instance = this;

    for (int i = 0; i < static_cast<int>(SoundEffect::MAX); i++) {
        m_sounds[i] = PLSR_PLAYER_INVALID_SOUND;
    }

    if (PLSR_RC_FAILED(plsrPlayerInit())) return;

    if (R_FAILED(romfsMountDataStorageFromProgram(0x0100000000001000, "qlaunch"))) {
        plsrPlayerExit();
        return;
    }

    if (PLSR_RC_FAILED(plsrBFSAROpen("qlaunch:/sound/qlaunch.bfsar", &m_bfsar))) {
        romfsUnmount("qlaunch");
        plsrPlayerExit();
        return;
    }

    for (const auto& mapping : SOUND_MAP) {
        plsrPlayerLoadSoundByName(&m_bfsar, mapping.name, &m_sounds[static_cast<int>(mapping.effect)]);
    }

    plsrBFSARClose(&m_bfsar);
    romfsUnmount("qlaunch");

    m_init = true;
}

Audio::~Audio() {
    if (m_init) {
        for (int i = 0; i < static_cast<int>(SoundEffect::MAX); i++) {
            if (m_sounds[i] != PLSR_PLAYER_INVALID_SOUND) plsrPlayerFree(m_sounds[i]);
        }
        plsrBFSARClose(&m_bfsar);
        plsrPlayerExit();
    }
    s_instance = nullptr;
}

void Audio::play(SoundEffect effect) {
    if (!m_init) return;
    int index = static_cast<int>(effect);
    if (index < 0 || index >= static_cast<int>(SoundEffect::MAX)) return;
    if (m_sounds[index] == PLSR_PLAYER_INVALID_SOUND) return;
    plsrPlayerPlay(m_sounds[index]);
}
