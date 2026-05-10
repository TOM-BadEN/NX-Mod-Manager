/**
 * MtpDialog - MTP 传输进度对话框
 *
 * 封装 MTP 传输的完整 UI 逻辑（对话框、进度条、速度/ETA 显示）。
 * 调用方只需一行 mtpDialog::open(mounts) 即可启动。
 * MtpManager 完全隐藏在 .cpp 内部，调用方不感知。
 */

#pragma once

#include <core/mtp.hpp>

namespace mtpDialog {
    /**
     * @brief 打开 MTP 传输对话框
     * @param mounts 挂载配置列表
     */
    void open(std::vector<MtpMount> mounts);
}
