/**
 * Help - 使用说明页面
 * 左侧 RecyclingGrid（说明条目标题） + 右侧动态内容
 */

#include "activity/help.hpp"
#include <borealis/core/i18n.hpp>
#include "dataSource/helpCardDS.hpp"
#include "view/helpCard.hpp"
#include "view/qrCode.hpp"
#include "common/config.hpp"
#include "core/audio.hpp"

Help::Help() {
    buildEntries();
    for (auto& entry : m_entries) {
        m_titles.push_back(entry.title);
    }
    m_entryPanels.resize(m_entries.size(), nullptr);
}

void Help::onContentAvailable() {
    m_grid->setPadding(25, 0, 25, 40);
    m_grid->setScrollingIndicatorVisible(false);
    m_grid->registerCell("HelpCard", HelpCard::create);
    m_grid->setDataSource(new HelpCardDS(m_titles));

    m_grid->setFocusChangeCallback([this](size_t index) {
        showEntry(index);
    });

    m_scroll->setScrollingIndicatorVisible(false);

    // 右键：列表 → 详情面板
    m_grid->registerAction("", brls::BUTTON_NAV_RIGHT, [this](...) {
        Audio::instance()->play(SoundEffect::Focus);
        brls::Application::giveFocus(m_detail);
        return true;
    }, true, true);

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

    if (!m_entries.empty()) showEntry(0);
}

void Help::showEntry(size_t index) {
    if (index >= m_entries.size()) return;
    m_lastFocusIndex = index;

    m_scroll->setContentOffsetY(0, false);
    auto*& panel = m_entryPanels[index];
    const bool isNewPanel = panel == nullptr;
    if (!panel) {
        panel = createEntryPanel(m_entries[index]);
    }

    if (m_visiblePanel == panel) return;

    if (m_visiblePanel) m_visiblePanel->setVisibility(brls::Visibility::GONE);
    if (isNewPanel)
        m_container->addView(panel);
    else
        panel->setVisibility(brls::Visibility::VISIBLE);
    m_visiblePanel = panel;
}

brls::Box* Help::createEntryPanel(const HelpEntry& entry) {
    auto* panel = new brls::Box();
    panel->setAxis(brls::Axis::COLUMN);
    panel->setWidthPercentage(100);

    for (const auto& text : entry.texts) {
        // 大文本（段标题）
        if (!text.title.empty()) {
            auto* titleLabel = new brls::Label();
            titleLabel->setFontSize(24);
            titleLabel->setText(text.title);
            titleLabel->setMargins(0, 0, 16, 0);
            panel->addView(titleLabel);
        }

        // 小文本（段正文）
        if (!text.content.empty()) {
            auto* contentLabel = new brls::Label();
            contentLabel->setFontSize(20);
            contentLabel->setText(text.content);
            auto theme = brls::Application::getTheme();
            contentLabel->setTextColor(theme.getColor("app/textSecondary"));
            contentLabel->setMargins(0, 0, 30, 0);
            panel->addView(contentLabel);
        }
    }

    // 二维码
    if (!entry.qrItems.empty()) {
        auto theme = brls::Application::getTheme();
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setJustifyContent(brls::JustifyContent::FLEX_START);
        row->setMargins(20, 0, 0, 0);

        for (size_t i = 0; i < entry.qrItems.size(); i++) {
            auto* card = new brls::Box();
            card->setAxis(brls::Axis::COLUMN);
            card->setAlignItems(brls::AlignItems::CENTER);
            card->setBackgroundColor(theme.getColor("app/qrCardBg"));
            card->setCornerRadius(15);
            card->setPadding(15, 15, 25, 15);
            if (i > 0) card->setMarginLeft(30);

            auto* qr = new QrCode();
            qr->setText(entry.qrItems[i].content);
            qr->setWidth(200);
            qr->setHeight(200);
            card->addView(qr);

            if (!entry.qrItems[i].label.empty()) {
                auto* label = new brls::Label();
                label->setText(entry.qrItems[i].label);
                label->setFontSize(20);
                label->setTextColor(theme.getColor("app/textDark"));
                label->setMargins(10, 0, 0, 0);
                card->addView(label);
            }

            row->addView(card);
        }

        panel->addView(row);
    }

    return panel;
}

HelpEntry& Help::addEntry(const std::string& title) {
    return m_entries.emplace_back(title);
}

void Help::buildEntries() {
    auto& a = addEntry(brls::getStr("help/entryIntro"));
    a.addText("NX Mod Manager", brls::getStr("help/introDesc"));
    a.addText(brls::getStr("help/introHighlights"), brls::getStr("help/introHighlightsDesc"));

    auto& ab = addEntry(brls::getStr("help/entryDisclaimer"));
    ab.addText(brls::getStr("help/disclaimerSoftware"), brls::getStr("help/disclaimerSoftwareDesc"));
    ab.addText(brls::getStr("help/disclaimerService"), brls::getStr("help/disclaimerServiceDesc"));
    ab.addText(brls::getStr("help/disclaimerCopyright"), brls::getStr("help/disclaimerCopyrightDesc"));

    auto& ac = addEntry(brls::getStr("help/entryDonate"));
    ac.addText(brls::getStr("help/donateStatement"), brls::getStr("help/donateStatementDesc"));
    ac.addQr(config::donateWechatQr, brls::getStr("help/qrWechat"));
    ac.addQr("https://qr.alipay.com/fkx14502q4ewflegde4xmd9", brls::getStr("help/qrAlipay"));
    ac.addQr(config::donatePaypalQr, brls::getStr("help/qrPaypal"));

    auto& ad = addEntry(brls::getStr("help/entryFeedback"));
    ad.addText(brls::getStr("help/feedbackInfo"), brls::getStr("help/feedbackInfoDesc"));
    ad.addText(brls::getStr("help/feedbackRequire"), brls::getStr("help/feedbackRequireDesc"));
    ad.addQr(config::projectGithubUrl, brls::getStr("help/qrGithub"));
    ad.addQr(config::communityQqQr, brls::getStr("help/qrQQ"));
    ad.addQr(config::communityDiscordQr, brls::getStr("help/qrDiscord"));

    auto& b = addEntry(brls::getStr("help/entryGuide"));
    b.addText(brls::getStr("help/guideIntro"), brls::getStr("help/guideIntroDesc"));
    b.addText(brls::getStr("help/guideAddGame"), brls::getStr("help/guideAddGameDesc"));
    b.addText(brls::getStr("help/guideTransfer"), brls::getStr("help/guideTransferDesc"));
    b.addText(brls::getStr("help/guideInstall"), brls::getStr("help/guideInstallDesc"));
    b.addQr(config::guideBilibiliUrl, brls::getStr("help/qrBilibili"));
    b.addQr(config::projectGithubUrl, brls::getStr("help/qrGithub"));

    auto& c = addEntry(brls::getStr("help/entryFaq"));
    c.addText(brls::getStr("help/faqModFormat"), brls::getStr("help/faqModFormatDesc"));
    c.addText(brls::getStr("help/faqTransfer"), brls::getStr("help/faqTransferDesc"));
    c.addText(brls::getStr("help/faqUpload"), brls::getStr("help/faqUploadDesc"));
    c.addText(brls::getStr("help/faqTransitEmpty"), brls::getStr("help/faqTransitEmptyDesc"));
    c.addText(brls::getStr("help/faqNetwork"), brls::getStr("help/faqNetworkDesc"));
    c.addText(brls::getStr("help/faqInvalidMod"), brls::getStr("help/faqInvalidModDesc"));
    c.addText(brls::getStr("help/faqConflict"), brls::getStr("help/faqConflictDesc"));
    c.addText(brls::getStr("help/faqRemoveGame"), brls::getStr("help/faqRemoveGameDesc"));
    c.addText(brls::getStr("help/faqDuplicateTid"), brls::getStr("help/faqDuplicateTidDesc"));
    c.addText(brls::getStr("help/faqForceClean"), brls::getStr("help/faqForceCleanDesc"));
    c.addText(brls::getStr("help/faqCustom"), brls::getStr("help/faqCustomDesc"));

}
