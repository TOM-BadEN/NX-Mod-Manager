#pragma once

#include <pulsar.h>

enum class SoundEffect {
    Focus,         // 焦点移动（SeGameIconFocus）
    FocusLimit,    // 焦点边界（SeGameIconLimit）
    Click,         // 点击（SeBtnDecide）
    Warning,       // 警告（SeInsertError）
    Enter,         // 进入（SeGameIconAdd）
    MAX,
};

class Audio {
public:
    Audio(bool muted = false);
    ~Audio();

    /**
     * @brief 播放音效
     * @param effect 音效类型
     */
    void play(SoundEffect effect);

    /** @brief 设置静音状态 */
    void setMuted(bool muted) { m_muted = muted; }

    /** @brief 获取全局单例 */
    static Audio* instance() { return s_instance; }

private:
    static inline Audio* s_instance = nullptr;                          // 全局单例指针
    bool m_init = false;                                                // 是否初始化成功
    bool m_muted = false;                                               // 是否静音
    PLSR_BFSAR m_bfsar{};                                               // 音效档案句柄
    PLSR_PlayerSoundId m_sounds[static_cast<int>(SoundEffect::MAX)]{};  // 音效 ID 数组
};
