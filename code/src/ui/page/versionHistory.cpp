/**
 * VersionHistory - 历史版本页面
 */

#include "ui/page/versionHistory.hpp"
#include "api/url.hpp"
#include "core/audio.hpp"
#include "ui/dataSource/versionHistoryCardDS.hpp"
#include "ui/view/versionHistoryCard.hpp"
#include "utils/format.hpp"
#include <borealis/core/i18n.hpp>
#include <utility>

VersionHistory::VersionHistory(std::vector<api::app::VersionHistoryItem> versions)
    : m_versions(std::move(versions)) {
    inflateFromXMLRes("xml/view/page/versionHistory.xml");

    setHeader();

    for (const auto& version : m_versions) {
        m_versionTags.push_back(format::cleanVersion(version.tagName));
    }
}

void VersionHistory::setHeader() {
    TitleState titleState;
    titleState.iconPath = "img/icon.jpg";
    titleState.title = brls::getStr("page/versionHistory/pageTitle");

    HeaderState headerState;
    headerState.setTitle(titleState);
    ShellState::setHeaderState(headerState);
}

void VersionHistory::onContentAvailable() {
    // B 键：返回
    registerAction(brls::getStr("page/versionHistory/back"), brls::BUTTON_B, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        Page::popPage();
        return true;
    });

    setupGrid();
    setupDetail();
    showVersion(0);

    m_layoutReady = true;
}

void VersionHistory::onLayout() {
    Page::onLayout();
    if (m_layoutReady) updateScrollHintVisibility();
}

void VersionHistory::setupGrid() {
    m_grid->setPadding(5, 0, 5, 40);
    m_grid->setScrollingIndicatorVisible(false);
    m_grid->registerCell("VersionHistoryCard", VersionHistoryCard::create);
    m_grid->setDataSource(new VersionHistoryCardDS(m_versionTags));

    m_grid->setFocusChangeCallback([this](size_t index) {
        ShellState::setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_versionTags.size()));
        showVersion(index);
    });

    // 右键：列表 → 详情面板
    m_grid->registerAction("", brls::BUTTON_NAV_RIGHT, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(m_detail);
        return true;
    }, true, true);
}

void VersionHistory::setupDetail() {
    // 焦点进入时显示滚动条，离开时显示底部提示
    m_scroll->setScrollingIndicatorVisible(false);
    m_scrollHint->setVisibility(brls::Visibility::INVISIBLE);
    m_detail->getFocusEvent()->subscribe([this](brls::View*) {
        m_scroll->setScrollingIndicatorVisible(true);
        updateScrollHintVisibility();
    });
    m_detail->getFocusLostEvent()->subscribe([this](brls::View*) {
        m_scroll->setScrollingIndicatorVisible(false);
        updateScrollHintVisibility();
    });

    // 左键：详情面板 → 列表（重置滚动 + 恢复焦点）
    m_detail->registerAction("", brls::BUTTON_NAV_LEFT, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        m_scroll->setContentOffsetY(0, false);
        auto* cell = m_grid->getGridItemByIndex(m_lastFocusIndex);
        if (cell) brls::Application::giveFocus(cell);
        else brls::Application::giveFocus(m_grid);
        return true;
    }, true, true);

    // 右键：详情面板边界
    m_detail->registerAction("", brls::BUTTON_NAV_RIGHT, [this](...) {
        Audio::instance()->play(SoundEffect::FocusLimit);
        m_detail->shakeHighlight(brls::FocusDirection::RIGHT);
        return true;
    }, true);

    // 上下键：驱动右侧滚动
    m_detail->registerAction("", brls::BUTTON_NAV_DOWN, [this](...) {
        float cur = m_scroll->getContentOffsetY();
        m_scroll->setContentOffsetY(cur + 60.0f, true);
        return true;
    }, true, true);
    m_detail->registerAction("", brls::BUTTON_NAV_UP, [this](...) {
        float cur = m_scroll->getContentOffsetY();
        m_scroll->setContentOffsetY(cur - 60.0f, true);
        return true;
    }, true, true);
}

void VersionHistory::updateScrollHintVisibility() {
    bool contentOverflows = m_scrollContent->getHeight() > m_scroll->getHeight();
    auto visibility = !m_detail->isFocused() && contentOverflows ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE;
    if (m_scrollHint->getVisibility() != visibility) m_scrollHint->setVisibility(visibility);
}

void VersionHistory::showVersion(size_t index) {
    m_lastFocusIndex = index;
    m_scroll->setContentOffsetY(0, false);

    const auto& version = m_versions[index];
    m_versionTitle->setText(m_versionTags[index]);
    m_publishTime->setText(brls::getStr("page/versionHistory/published") + format::timeAgo(version.publishTime));
    const std::string& notes = releaseNotes(version);
    m_releaseNotes->setText(notes.empty() ? brls::getStr("page/versionHistory/noChangelog") : notes);
    updateScrollHintVisibility();
}

const std::string& VersionHistory::releaseNotes(const api::app::VersionHistoryItem& version) const {
    auto lang = api::url::getLang();
    if (lang == "zh-CN" || lang == "zh-TW") return version.releaseNotesZh;
    return version.releaseNotesEn;
}
