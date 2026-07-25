/**
 * Audio - 全局 UI 音效模块
 *
 * 从系统 qlaunch 音效档案中加载应用使用的 UI 音效，并通过全局实例
 * 向页面和组件提供统一的播放与静音控制。
 */

#pragma once

#include <pulsar.h>

/** @brief 应用使用的 UI 音效类型 */
enum class SoundEffect {
    Focus,         // 焦点移动（SeGameIconFocus）
    FocusLimit,    // 焦点边界（SeGameIconLimit）
    Click,         // 点击（SeBtnDecide）
    Warning,       // 警告（SeInsertError）
    Enter,         // 进入（SeGameIconAdd）
    Launch,        // 启动游戏（SeGameIconDecide）
    MAX,           // 音效类型数量
};

/** @brief 负责加载、播放和释放全局 UI 音效 */
class Audio {
public:
    /**
     * @brief 初始化音效系统
     * @param muted 是否以静音状态启动
     */
    Audio(bool muted = false);

    /** @brief 释放已加载的音效和播放器资源 */
    ~Audio();

    /**
     * @brief 播放音效
     * @param effect 音效类型
     */
    void play(SoundEffect effect);

    /**
     * @brief 设置静音状态
     * @param muted 是否静音
     */
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
