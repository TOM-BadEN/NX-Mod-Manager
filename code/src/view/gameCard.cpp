/**
 * GameCard - 游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 mod 数量
 */

#include "view/gameCard.hpp"
#include <borealis/core/i18n.hpp>
#include <borealis/views/hint.hpp>
#include "core/audio.hpp"
#include "core/device.hpp"
#include "utils/format.hpp"

// 构造函数：加载 XML 布局
GameCard::GameCard() {
    inflateFromXMLRes("xml/view/gameCard.xml");
    m_icon->setFreeTexture(false);
    m_defaultIconId = m_icon->getTexture();
    m_icon->setVisibility(brls::Visibility::INVISIBLE);
    m_launchIcon->setText(brls::Hint::getKeyIcon(brls::BUTTON_RSB));
    m_launchHint->setTranslationX(LAUNCH_HINT_HIDDEN_X);
    m_launchHintX.setTickCallback([this] { m_launchHint->setTranslationX(m_launchHintX.getValue()); });
    m_launchScale.setEndCallback([this](bool finished) {
        if (finished && m_launchAction) m_launchAction();
        m_launchText->setText(brls::getStr("views/gameCard/launch"));
        deviceControl::HomeButton::enable();
        brls::Application::unblockInputs();
    });
    
    registerAction("", brls::BUTTON_RSB, [this](...) {
        if (!m_launchAvailable) return false;
        if (m_launchScale.isRunning()) return true;
        m_launchText->setText(brls::getStr("views/gameCard/launching"));
        deviceControl::HomeButton::disable();
        brls::Application::blockInputs();
        Audio::instance()->play(SoundEffect::Launch);
        animateLaunchPress();
        return true;
    });
}

void GameCard::onFocusGained() {
    RecyclingGridItem::onFocusGained();
    if (m_launchAvailable) animateLaunchHint(true);
}

void GameCard::onFocusLost() {
    animateLaunchHint(false);
    RecyclingGridItem::onFocusLost();
}

void GameCard::draw(NVGcontext* vg, float x, float y, float width, float height,
                    brls::Style style, brls::FrameContext* ctx) {
    float scale = m_launchScale.getValue();
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
    nvgRestore(vg);

    RecyclingGridItem::draw(vg, x, y, width, height, style, ctx);
}

void GameCard::animateLaunchHint(bool visible) {
    if (!visible) {
        m_launchHintX.reset(LAUNCH_HINT_HIDDEN_X);
        m_launchHint->setTranslationX(LAUNCH_HINT_HIDDEN_X);
        m_launchHint->setVisibility(brls::Visibility::INVISIBLE);
        return;
    }

    float currentX = m_launchHintX.getValue();
    m_launchHintX.reset(currentX);
    m_launchHint->setVisibility(brls::Visibility::VISIBLE);
    m_launchHintX.addStep(0.0f, 220, brls::EasingFunction::quadraticInOut);
    m_launchHintX.start();
}

void GameCard::animateLaunchPress() {
    m_launchScale.reset(1.0f);
    m_launchScale.addStep(0.95f, 110, brls::EasingFunction::quadraticOut);
    m_launchScale.addStep(1.0f, 190, brls::EasingFunction::quadraticOut);
    m_launchScale.start();
}

// 设置卡片数据
void GameCard::setGame(const std::string& name, const std::string& version, const std::string& modCount) {
    m_name->setText(name.empty() ? brls::getStr("views/gameCard/virtualGame") : name);
    m_version->setText(format::cleanVersion(version));
    m_modCount->setText(modCount.empty() || modCount == "0" ? "0" : modCount);
}

void GameCard::setLaunchAvailable(bool available) {
    m_launchAvailable = available;
    if (!available) animateLaunchHint(false);
}

void GameCard::setLaunchAction(std::function<void()> action) {
    m_launchAction = std::move(action);
}

// 设置游戏图标（传入 NVG 纹理 ID）
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
    m_launchAvailable = false;
    m_launchHintX.reset(LAUNCH_HINT_HIDDEN_X);
    m_launchScale.reset(1.0f);
    m_launchAction = nullptr;
    m_launchHint->setTranslationX(LAUNCH_HINT_HIDDEN_X);
    m_launchHint->setVisibility(brls::Visibility::INVISIBLE);
    m_launchText->setText(brls::getStr("views/gameCard/launch"));

    resetIcon();
    m_like->setVisibility(brls::Visibility::GONE);
    m_name->setText("");
    m_name->setTextColor(brls::Application::getTheme()["app/cardOverlayText"]);
    m_version->setText("");
    m_modCount->setText("");
}

RecyclingGridItem* GameCard::create() {
    return new GameCard();
}
