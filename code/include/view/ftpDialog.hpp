/**
 * FtpDialog - FTP 传输状态对话框
 *
 * 封装 FTP 传输的完整 UI 逻辑（对话框、状态显示、文件计数）。
 * 调用方只需一行 ftpDialog::open(mounts) 即可启动。
 * FtpManager 完全隐藏在 .cpp 内部，调用方不感知。
 */

#pragma once

#include <core/ftp.hpp>

namespace ftpDialog {
    /**
     * @brief 打开 FTP 传输对话框
     * @param mounts 挂载配置列表
     */
    void open(std::vector<FtpMount> mounts);
}
