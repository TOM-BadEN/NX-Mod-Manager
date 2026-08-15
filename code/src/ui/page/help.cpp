/**
 * Help - 使用说明页面
 */

#include "ui/page/help.hpp"
#include "common/config.hpp"
#include "core/audio.hpp"
#include "ui/dataSource/helpCardDS.hpp"
#include "ui/view/helpCard.hpp"
#include "ui/view/qrCode.hpp"
#include "ui/view/sectionTitle.hpp"
#include <borealis/core/i18n.hpp>

Help::Help() {
    inflateFromXMLRes("xml/view/page/help.xml");

    setHeader();

    buildEntries();
    for (auto& entry : m_entries) {
        m_titles.push_back(entry.title);
    }
    m_entryPanels.resize(m_entries.size(), nullptr);
}

void Help::setHeader() {
    TitleState titleState;
    titleState.iconPath = "img/icon.jpg";
    titleState.title = brls::getStr("page/help/pageTitle");

    HeaderState headerState;
    headerState.setTitle(titleState);
    ShellState::setHeaderState(headerState);
}

void Help::onContentAvailable() {
    // B 键：返回
    registerAction(brls::getStr("page/help/back"), brls::BUTTON_B, [this](...) {
        Audio::instance()->play(SoundEffect::Enter);
        Page::popPage();
        return true;
    });

    m_grid->setPadding(5, 0, 5, 40);
    m_grid->setScrollingIndicatorVisible(false);
    m_grid->registerCell("HelpCard", HelpCard::create);
    m_grid->setDataSource(new HelpCardDS(m_titles));

    m_grid->setFocusChangeCallback([this](size_t index) {
        ShellState::setIndexText(std::to_string(index + 1) + " / " + std::to_string(m_titles.size()));
        showEntry(index);
    });

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

    m_layoutReady = true;
}

void Help::onLayout() {
    Page::onLayout();
    if (m_layoutReady) updateScrollHintVisibility();
}

void Help::updateScrollHintVisibility() {
    bool contentOverflows = m_container->getHeight() > m_scroll->getHeight();
    auto visibility = !m_detail->isFocused() && contentOverflows ? brls::Visibility::VISIBLE : brls::Visibility::INVISIBLE;
    if (m_scrollHint->getVisibility() != visibility) m_scrollHint->setVisibility(visibility);
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
    updateScrollHintVisibility();
}

brls::Box* Help::createEntryPanel(const HelpEntry& entry) {
    auto* panel = new brls::Box();
    panel->setAxis(brls::Axis::COLUMN);
    panel->setWidthPercentage(100);

    for (const auto& text : entry.texts) {
        // 大文本（段标题）
        if (!text.title.empty()) {
            auto* titleRow = new SectionTitle();
            titleRow->setMarginBottom(16);

            auto* titleLabel = new brls::Label();
            titleLabel->setFontSize(24);
            titleLabel->setText(text.title);
            titleRow->addView(titleLabel);
            panel->addView(titleRow);
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
            card->setBorderColor(theme.getColor("app/helpQrCardBorder"));
            card->setBorderThickness(1);
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
    auto& a = addEntry(brls::getStr("page/help/entryIntro"));
    a.addText("NX Mod Manager", brls::getStr("page/help/introDesc"));
    a.addText(brls::getStr("page/help/introHighlights"), brls::getStr("page/help/introHighlightsDesc"));

    auto& ab = addEntry(brls::getStr("page/help/entryDisclaimer"));
    ab.addText(brls::getStr("page/help/disclaimerSoftware"), brls::getStr("page/help/disclaimerSoftwareDesc"));
    ab.addText(brls::getStr("page/help/disclaimerService"), brls::getStr("page/help/disclaimerServiceDesc"));
    ab.addText(brls::getStr("page/help/disclaimerCopyright"), brls::getStr("page/help/disclaimerCopyrightDesc"));

    auto& ac = addEntry(brls::getStr("page/help/entryDonate"));
    ac.addText(brls::getStr("page/help/donateStatement"), brls::getStr("page/help/donateStatementDesc"));
    ac.addQr(config::donateWechatQr, brls::getStr("page/help/qrWechat"));
    ac.addQr("https://qr.alipay.com/fkx14502q4ewflegde4xmd9", brls::getStr("page/help/qrAlipay"));
    ac.addQr(config::donatePaypalQr, brls::getStr("page/help/qrPaypal"));

    auto& ad = addEntry(brls::getStr("page/help/entryFeedback"));
    ad.addText(brls::getStr("page/help/feedbackInfo"), brls::getStr("page/help/feedbackInfoDesc"));
    ad.addText(brls::getStr("page/help/feedbackRequire"), brls::getStr("page/help/feedbackRequireDesc"));
    ad.addQr(config::projectGithubUrl, brls::getStr("page/help/qrGithub"));
    ad.addQr(config::communityQqQr, brls::getStr("page/help/qrQQ"));
    ad.addQr(config::communityDiscordQr, brls::getStr("page/help/qrDiscord"));

    auto& b = addEntry(brls::getStr("page/help/entryGuide"));
    b.addText(brls::getStr("page/help/guideIntro"), brls::getStr("page/help/guideIntroDesc"));
    b.addText(brls::getStr("page/help/guideAddGame"), brls::getStr("page/help/guideAddGameDesc"));
    b.addText(brls::getStr("page/help/guideTransfer"), brls::getStr("page/help/guideTransferDesc"));
    b.addText(brls::getStr("page/help/guideInstall"), brls::getStr("page/help/guideInstallDesc"));
    b.addQr(config::guideBilibiliUrl, brls::getStr("page/help/qrBilibili"));
    b.addQr(config::projectGithubUrl, brls::getStr("page/help/qrGithub"));

    auto& c = addEntry(brls::getStr("page/help/entryFaq"));
    c.addText(brls::getStr("page/help/faqModFormat"), brls::getStr("page/help/faqModFormatDesc"));
    c.addText(brls::getStr("page/help/faqTransfer"), brls::getStr("page/help/faqTransferDesc"));
    c.addText(brls::getStr("page/help/faqUpload"), brls::getStr("page/help/faqUploadDesc"));
    c.addText(brls::getStr("page/help/faqTransitEmpty"), brls::getStr("page/help/faqTransitEmptyDesc"));
    c.addText(brls::getStr("page/help/faqNetwork"), brls::getStr("page/help/faqNetworkDesc"));
    c.addText(brls::getStr("page/help/faqInvalidMod"), brls::getStr("page/help/faqInvalidModDesc"));
    c.addText(brls::getStr("page/help/faqConflict"), brls::getStr("page/help/faqConflictDesc"));
    c.addText(brls::getStr("page/help/faqRemoveGame"), brls::getStr("page/help/faqRemoveGameDesc"));
    c.addText(brls::getStr("page/help/faqDuplicateTid"), brls::getStr("page/help/faqDuplicateTidDesc"));
    c.addText(brls::getStr("page/help/faqForceClean"), brls::getStr("page/help/faqForceCleanDesc"));
    c.addText(brls::getStr("page/help/faqCustom"), brls::getStr("page/help/faqCustomDesc"));
}
