/**
 * AppUpdater - 应用更新管理
 * 单例，负责版本检查、结果存储、下载新版本并替换原 NRO 文件
 */

#pragma once

#include <string>
#include <stop_token>
#include "utils/http.hpp"
#include "api/app.hpp"

class AppUpdater {
public:
    static AppUpdater& instance();

    /** @brief 获取启动检查的取消令牌（供 main.cpp 提交线程池时使用） */
    std::stop_token token() { return m_stopSource.get_token(); }

    /**
     * @brief 检查更新（同步，自动取消上一次未完成的检查）
     * @param token 调用方的取消令牌
     * @param localVersion 当前版本号（如 APP_VERSION）
     */
    void check(std::stop_token token, const std::string& localVersion);

    /**
     * @brief 下载新版本 NRO 到临时目录
     * @param token 取消令牌
     * @param progressCb 进度回调 (total, now) → 返回 false 可中断
     * @return HTTP 响应
     */
    http::Response download(std::stop_token token, http::ProgressCallback progressCb);

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

    /** @brief 将检查结果写入成员变量 */
    void applyResult(api::app::LatestResult& result, const std::string& localVersion);

    std::stop_source m_stopSource;      // 启动检查的取消源
    bool m_firstCheck = true;             // 首次检查跳过取消（避免取消自己的 token）
    std::string m_tagName;              // 新版本号
    std::string m_publishTime;          // 发布时间
    std::string m_releaseNotesZh;       // 中文更新日志
    std::string m_releaseNotesEn;       // 英文更新日志
    bool m_hasUpdate = false;           // 是否有更新
};
