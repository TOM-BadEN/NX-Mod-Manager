/**
 * GameCard - 游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 mod 数量
 */

#include "ui/view/gameCard.hpp"
#include "core/audio.hpp"
#include "core/device.hpp"
#include "utils/format.hpp"
#include <borealis/core/i18n.hpp>
#include <borealis/views/hint.hpp>
#include <utility>

GameCard::GameCard() {
    inflateFromXMLRes("xml/view/gameCard.xml");
    m_icon->setFreeTexture(false);
    m_defaultIconId = m_icon->getTexture();
    m_icon->setVisibility(brls::Visibility::INVISIBLE);
    m_launchIcon->setText(brls::Hint::getKeyIcon(brls::BUTTON_RSB));
    m_launchHint->setTranslationX(LAUNCH_HINT_HIDDEN_X);
    m_launchHintX.setTickCallback([this] { m_launchHint->setTranslationX(m_launchHintX.getValue()); });
    m_iconScale.setEndCallback([this](bool finished) {
        if (!m_launching) return;
        m_launching = false;
        if (finished && m_launchAction) m_launchAction();
        m_launchText->setText(brls::getStr("view/gameCard/launch"));
        deviceControl::HomeButton::enable();
        brls::Application::unblockInputs();
    });

    registerAction("", brls::BUTTON_RSB, [this](...) {
        if (!m_launchAvailable) return false;
        if (m_launching) return true;
        m_launchText->setText(brls::getStr("view/gameCard/launching"));
        deviceControl::HomeButton::disable();
        brls::Application::blockInputs();
        Audio::instance()->play(SoundEffect::Launch);
        animateLaunchPress();
        return true;
    }, true);
}

GameCard::~GameCard() {
    resetIcon();
}

void GameCard::onFocusGained() {
    RecyclingGridItem::onFocusGained();
    updateFocusVisuals(true);
}

void GameCard::onFocusLost() {
    updateFocusVisuals(false);
    RecyclingGridItem::onFocusLost();
}

void GameCard::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    float scale = m_iconScale.getValue();
    float iconX = m_icon->getX();
    float iconY = m_icon->getY();
    float iconWidth = m_icon->getWidth();
    float iconHeight = m_icon->getHeight();
    float centerX = iconX + iconWidth * 0.5f;
    float centerY = iconY + iconHeight * 0.5f;

    nvgSave(vg);
    nvgTranslate(vg, centerX, centerY);
    nvgScale(vg, scale, scale);
    nvgTranslate(vg, -centerX, -centerY);
    m_icon->draw(vg, iconX, iconY, iconWidth, iconHeight, style, ctx);
    m_mask->draw(vg, m_mask->getX(), m_mask->getY(), m_mask->getWidth(), m_mask->getHeight(), style, ctx);
    nvgRestore(vg);

    RecyclingGridItem::draw(vg, x, y, width, height, style, ctx);
}

void GameCard::updateLaunchHint(bool visible) {
    if (!visible) {
        m_launchHintX.reset(LAUNCH_HINT_HIDDEN_X);
        m_launchHint->setTranslationX(LAUNCH_HINT_HIDDEN_X);
        m_launchHint->setVisibility(brls::Visibility::INVISIBLE);
        return;
    }

    m_launchHintX.reset();
    m_launchHint->setVisibility(brls::Visibility::VISIBLE);
    m_launchHintX.addStep(0.0f, 220, brls::EasingFunction::quadraticInOut);
    m_launchHintX.start();
}

void GameCard::animateLaunchPress() {
    m_iconScale.reset();
    m_launching = true;
    m_iconScale.addStep(PRESSED_ICON_SCALE, 110, brls::EasingFunction::quadraticOut);
    m_iconScale.addStep(FOCUS_ICON_SCALE, 190, brls::EasingFunction::quadraticOut);
    m_iconScale.start();
}

void GameCard::updateFocusVisuals(bool focused) {
    float targetScale = focused ? FOCUS_ICON_SCALE : 1.0f;
    brls::Style style = brls::Application::getStyle();
    m_iconScale.reset();
    m_iconScale.addStep(targetScale, style["brls/animations/highlight"], brls::EasingFunction::quadraticOut);
    m_iconScale.start();
    if (m_launchAvailable) updateLaunchHint(focused);
}

void GameCard::setGame(const std::string& name, const std::string& version, const std::string& modCount) {
    m_name->setText(name);
    m_version->setText(format::cleanVersion(version));
    m_modCount->setText(modCount.empty() || modCount == "0" ? "0" : modCount);
}

void GameCard::setLaunchAvailable(bool available) {
    m_launchAvailable = available;
}

void GameCard::setLaunchAction(std::function<void()> action) {
    m_launchAction = std::move(action);
}

void GameCard::setIcon(int iconId) {
    if (iconId <= 0) return;
    m_icon->innerSetImage(iconId);
}

void GameCard::setFavorite(bool favorite) {
    m_like->setVisibility(favorite ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

void GameCard::setNameColor(NVGcolor color) {
    m_name->setTextColor(color);
}

void GameCard::resetIcon() {
    m_icon->innerSetImage(m_defaultIconId);
}

void GameCard::prepareForReuse() {
    m_iconScale.reset(1.0f);
    updateLaunchHint(false);

    resetIcon();
    m_name->setTextColor(brls::Application::getTheme()["app/cardOverlayText"]);
}

RecyclingGridItem* GameCard::create() {
    return new GameCard();
}
