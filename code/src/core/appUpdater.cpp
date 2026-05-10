/**
 * AppUpdater - 应用更新管理实现
 */

#include "core/appUpdater.hpp"
#include "api/url.hpp"
#include "common/config.hpp"
#include "utils/format.hpp"
#include <cstdio>
#include <switch.h>

// ── AppUpdater ───────────────────────────────────────────

AppUpdater& AppUpdater::instance() {
    static AppUpdater s;
    return s;
}

void AppUpdater::check(const std::string& localVersion) {
    m_task = util::async([this, localVersion](std::stop_token token) {
        auto result = api::app::fetchLatest(token);
        if (token.stop_requested()) return;
        if (!result.success) return;
        applyResult(result, localVersion);
    });
}

void AppUpdater::checkSync(std::stop_token token, const std::string& localVersion) {
    m_task.request_stop();
    m_task = util::AsyncFurture<void>();
    auto result = api::app::fetchLatest(token);
    if (token.stop_requested()) return;
    if (!result.success) return;
    applyResult(result, localVersion);
}

void AppUpdater::applyResult(api::app::LatestResult& result, const std::string& localVersion) {
    m_tagName        = std::move(result.tagName);
    m_publishTime    = format::timeAgo(result.publishTime);
    m_releaseNotesZh = std::move(result.releaseNotesZh);
    m_releaseNotesEn = std::move(result.releaseNotesEn);
    m_hasUpdate      = format::versionCode(m_tagName) > format::versionCode(localVersion);
}

http::Response AppUpdater::download(std::stop_token token, http::ProgressCallback progressCb) {
    std::string tmpPath = std::string(config::appUpdateDir) + "temp/update.nro";
    return http::downloadToFile(api::url::app::download(), tmpPath, progressCb, token);
}

bool AppUpdater::install() {
    std::string nroPath = config::getNroPath();
    std::string tmpPath = std::string(config::appUpdateDir) + "temp/update.nro";

    // 释放 romfs 挂载，解除对运行中 NRO 文件的读句柄锁定
    romfsExit();

    // 替换原文件（FAT 文件系统 rename 不能覆盖，需要先删）
    std::remove(nroPath.c_str());
    if (std::rename(tmpPath.c_str(), nroPath.c_str()) != 0) {
        std::remove(tmpPath.c_str());
        return false;
    }
    return true;
}

bool AppUpdater::hasUpdate() const {
    return m_hasUpdate;
}

const std::string& AppUpdater::tagName() const {
    return m_tagName;
}

const std::string& AppUpdater::publishTime() const {
    return m_publishTime;
}

const std::string& AppUpdater::releaseNotes() const {
    static const std::string empty;
    auto lang = api::url::getLang();
    if (lang == "zh-CN" || lang == "zh-TW") return m_releaseNotesZh.empty() ? empty : m_releaseNotesZh;
    return m_releaseNotesEn.empty() ? empty : m_releaseNotesEn;
}
