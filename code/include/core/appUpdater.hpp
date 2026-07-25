/**
 * AppUpdater - 应用更新管理
 * 单例，负责版本检查、结果存储、下载新版本并替换原 NRO 文件
 */

#pragma once

#include "api/app.hpp"
#include <cstddef>
#include <functional>
#include <stop_token>
#include <string>

class AppUpdater {
public:
    /** @brief 获取应用更新管理单例 */
    static AppUpdater& instance();

    /**
     * @brief 检查更新
     * @param token 调用方的取消令牌
     * @param localVersion 当前版本号（如 APP_VERSION）
     */
    void check(std::stop_token token, const std::string& localVersion);

    /**
     * @brief 下载新版本 NRO 到临时目录
     * @param token 取消令牌
     * @param progressCb 进度回调 (total, now)，返回 false 可中断
     * @return ApiResult 包含成功、失败状态和错误信息
     */
    api::ApiResult download(std::stop_token token, std::function<bool(size_t total, size_t now)> progressCb);

    /**
     * @brief 释放 romfs 并用下载的临时文件覆盖运行中的 NRO
     * @note 调用后 romfs:/ 不可用，应立即重启
     * @return 是否替换成功
     */
    bool install();

    /** @brief 是否检测到新版本 */
    bool hasUpdate() const;

    /** @brief 新版本号（如 "v3.2.0"） */
    const std::string& tagName() const;

    /** @brief 发布时间（相对时间，如 "1天前"） */
    const std::string& publishTime() const;

    /** @brief 更新日志（根据系统语言返回中/英） */
    const std::string& releaseNotes() const;

private:
    AppUpdater() = default;

    /**
     * @brief 将检查结果写入成员变量
     * @param result 最新版本查询结果
     * @param localVersion 当前版本号
     */
    void applyResult(api::app::LatestResult& result, const std::string& localVersion);

    std::string m_tagName;        // 新版本号
    std::string m_publishTime;    // 发布时间
    std::string m_releaseNotesZh; // 中文更新日志
    std::string m_releaseNotesEn; // 英文更新日志
    bool m_hasUpdate = false;     // 是否有更新
};
