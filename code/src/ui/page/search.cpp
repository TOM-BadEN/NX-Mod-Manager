/**
 * Search - 搜索页面
 */

#include "ui/page/search.hpp"
#include "core/audio.hpp"
#include <borealis/core/i18n.hpp>
#include <utility>

namespace {

int utf8CharCount(const std::string& text) {
    int count = 0;
    for (size_t i = 0; i < text.size();) {
        unsigned char character = static_cast<unsigned char>(text[i]);
        if (character < 0x80) i += 1;
        else if ((character & 0xE0) == 0xC0) i += 2;
        else if ((character & 0xF0) == 0xE0) i += 3;
        else i += 4;
        count++;
    }
    return count;
}

/** @brief 将行内焦点导航交给搜索结果网格处理 */
class ResultPassthroughRow : public brls::Box {
public:
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View*) override
    {
        if (hasParent()) return getParent()->getNextFocus(direction, this);
        return nullptr;
    }
};

constexpr float BUTTON_WIDTH = 388.0f;
constexpr float BUTTON_SPACING = 18.0f;
constexpr int BUTTONS_PER_ROW = 3;

} // namespace

void ResultButton::onFocusGained() {
    setHideHighlight(false);
    setHideClickAnimation(false);
    brls::Box::onFocusGained();
}

void ResultButton::onFocusLost() {
    setHideHighlight(true);
    playClickAnimation(false, false, true);
    resetClickAnimation();
    setHideClickAnimation(true);
    brls::Box::onFocusLost();
}

void ResultButtonGrid::setButtons(std::vector<ResultButton*>* buttons) {
    m_buttons = buttons;
}

brls::View* ResultButtonGrid::getNextFocus(brls::FocusDirection direction, brls::View* currentView)
{
    if (!m_buttons || m_buttons->empty()) return brls::Box::getNextFocus(direction, currentView);

    auto* focused = brls::Application::getCurrentFocus();
    int current = -1;
    for (size_t i = 0; i < m_buttons->size(); i++) {
        if ((*m_buttons)[i] == focused) {
            current = static_cast<int>(i);
            break;
        }
    }
    if (current < 0) return brls::Box::getNextFocus(direction, currentView);

    int count = static_cast<int>(m_buttons->size());
    constexpr int columns = 3;
    int next = -1;

    if (direction == brls::FocusDirection::LEFT) {
        int candidate = (current + count - 1) % count;
        if (candidate != current) next = candidate;
    } else if (direction == brls::FocusDirection::RIGHT) {
        int candidate = (current + 1) % count;
        if (candidate != current) next = candidate;
    } else if (direction == brls::FocusDirection::UP) {
        if (current >= columns) next = current - columns;
    } else if (direction == brls::FocusDirection::DOWN) {
        if (current + columns < count) next = current + columns;
    }

    if (next >= 0) {
        Audio::instance()->play(SoundEffect::Focus);
        return (*m_buttons)[next];
    }

    Audio::instance()->play(SoundEffect::FocusLimit);
    return (*m_buttons)[current];
}

brls::View* ResultButtonGrid::create() {
    return new ResultButtonGrid();
}

Search::Search(const std::vector<std::string>& items, std::function<void(int)> onSelect)
    : m_items(items)
    , m_onSelect(std::move(onSelect)) {
    inflateFromXMLRes("xml/view/page/search.xml");

    ShellState::setTitle(brls::getStr("page/search/pageTitle"));
}

void Search::onContentAvailable() {
    // ZR：在虚拟键盘和搜索结果之间切换焦点
    registerAction(brls::getStr("page/search/switchResult"), brls::ControllerButton::BUTTON_RT, [this](brls::View*) {
        Audio::instance()->play(SoundEffect::Click);
        if (isFocusInResults()) switchToKeyboard();
        else switchToResults();
        return true;
    });
    setActionAvailable(brls::ControllerButton::BUTTON_RT, false);

    // X：返回调用搜索的页面
    registerAction(brls::getStr("page/search/back"), brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        Page::popPage();
        return true;
    });

    // B：键盘区域用于删除，结果区域吞掉事件，避免触发全局返回
    registerAction("", brls::ControllerButton::BUTTON_B, [](brls::View*) {
        return true;
    }, true);

    int maxLength = m_keyboard->getMaxLength();
    ShellState::setIndexText("0 / " + std::to_string(maxLength));
    m_keyboard->getTextChangeEvent()->subscribe([this, maxLength](const std::string& text) {
        updateResults(text);
        ShellState::setIndexText(std::to_string(utf8CharCount(text)) + " / " + std::to_string(maxLength));
    });
}

void Search::showHint(const std::string& text) {
    m_hint->setText(text);
    m_hint->setVisibility(brls::Visibility::VISIBLE);
    m_buttonContainer->setVisibility(brls::Visibility::GONE);
    setActionAvailable(brls::ControllerButton::BUTTON_RT, false);
}

bool Search::isFocusInResults() {
    auto* view = brls::Application::getCurrentFocus();
    while (view) {
        if (view == m_buttonContainer) return true;
        view = view->getParent();
    }
    return false;
}

void Search::switchToResults() {
    if (m_resultButtons.empty()) return;

    m_lastKeyboardFocus = brls::Application::getCurrentFocus();
    brls::Application::giveFocus(m_resultButtons[0]);
    updateActionHint(brls::ControllerButton::BUTTON_RT, brls::getStr("page/search/switchKeyboard"));
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void Search::switchToKeyboard() {
    auto* target = m_lastKeyboardFocus
        ? m_lastKeyboardFocus
        : m_keyboard->getDefaultFocus();
    if (target) brls::Application::giveFocus(target);

    updateActionHint(brls::ControllerButton::BUTTON_RT, brls::getStr("page/search/switchResult"));
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void Search::updateResults(const std::string& keyword) {
    if (isFocusInResults()) switchToKeyboard();

    m_resultButtons.clear();
    m_buttonContainer->clearViews();

    if (keyword.empty()) {
        showHint(brls::getStr("page/search/hintIdle"));
        return;
    }

    auto results = m_searchEngine.search(keyword, m_items, 6);
    if (results.empty()) {
        showHint(brls::getStr("page/search/hintEmpty"));
        return;
    }

    m_hint->setVisibility(brls::Visibility::GONE);
    m_buttonContainer->setVisibility(brls::Visibility::VISIBLE);
    setActionAvailable(brls::ControllerButton::BUTTON_RT, true);

    brls::Box* currentRow = nullptr;
    for (size_t i = 0; i < results.size(); i++) {
        if (i % BUTTONS_PER_ROW == 0) {
            currentRow = new ResultPassthroughRow();
            currentRow->setAxis(brls::Axis::ROW);
            currentRow->setJustifyContent(brls::JustifyContent::FLEX_START);
            if (i > 0) currentRow->setMarginTop(BUTTON_SPACING);
            m_buttonContainer->addView(currentRow);
        }

        auto* button = createResultButton(results[i]);
        if (i % BUTTONS_PER_ROW > 0) button->setMarginLeft(BUTTON_SPACING);
        currentRow->addView(button);
        m_resultButtons.push_back(button);
    }

    m_buttonContainer->setButtons(&m_resultButtons);
}

ResultButton* Search::createResultButton(const SearchEngine::Result& result)
{
    auto theme = brls::Application::getTheme();

    auto* button = new ResultButton();
    button->setFocusable(true);
    button->setWidth(BUTTON_WIDTH);
    button->setAlignItems(brls::AlignItems::CENTER);
    button->setJustifyContent(brls::JustifyContent::CENTER);
    button->setPadding(20, 30, 20, 30);
    button->setBackgroundColor(theme.getColor("app/cardBg"));
    button->setCornerRadius(8);
    button->setHighlightCornerRadius(8);
    button->setShadowType(brls::ShadowType::GENERIC);

    auto* label = new brls::Label();
    label->setSingleLine(true);
    label->setFontSize(23);
    label->setText(result.name);
    button->addView(label);

    int index = result.index;
    button->registerAction(brls::getStr("page/search/confirm"), brls::BUTTON_A, [this, index](...) {
        Audio::instance()->play(SoundEffect::Enter);

        auto callback = m_onSelect;
        Page::popPage();
        if (callback) callback(index);
        return true;
    }, false, false);

    button->addGestureRecognizer(new brls::TapGestureRecognizer(button));
    return button;
}
