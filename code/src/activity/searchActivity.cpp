/**
 * SearchActivity - 搜索页面
 * 独立 Activity，内置虚拟键盘 + 搜索结果列表
 */

#include "activity/searchActivity.hpp"
#include <borealis/core/i18n.hpp>
#include "core/audio.hpp"

// ── ResultButton ─────────────────────────────────────────────

void ResultButton::onFocusGained() {
    setHideHighlight(false);
    setHideClickAnimation(false);
    Box::onFocusGained();
}

void ResultButton::onFocusLost() {
    setHideHighlight(true);
    playClickAnimation(false, false, true);
    resetClickAnimation();
    setHideClickAnimation(true);
    Box::onFocusLost();
}

// ── ResultButtonGrid ─────────────────────────────────────────

// 行内透传容器：不解析焦点，委托给 ResultButtonGrid 处理
class ResultPassthroughRow : public brls::Box {
public:
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override {
        if (hasParent()) return getParent()->getNextFocus(direction, this);
        return nullptr;
    }
};

brls::View* ResultButtonGrid::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    if (!m_buttons || m_buttons->empty()) return Box::getNextFocus(direction, currentView);

    auto* focused = brls::Application::getCurrentFocus();
    int current = -1;
    for (size_t i = 0; i < m_buttons->size(); i++) {
        if ((*m_buttons)[i] == focused) { current = static_cast<int>(i); break; }
    }
    if (current < 0) return Box::getNextFocus(direction, currentView);

    int count = static_cast<int>(m_buttons->size());
    static constexpr int COLS = 3;
    int next = -1;

    using D = brls::FocusDirection;
    if (direction == D::LEFT) {
        int candidate = (current + count - 1) % count;
        if (candidate != current) next = candidate;
    } else if (direction == D::RIGHT) {
        int candidate = (current + 1) % count;
        if (candidate != current) next = candidate;
    } else if (direction == D::UP) {
        if (current >= COLS) next = current - COLS;
    } else if (direction == D::DOWN) {
        if (current + COLS < count) next = current + COLS;
    }

    if (next >= 0) {
        Audio::instance()->play(SoundEffect::Focus);
        return (*m_buttons)[next];
    }
    Audio::instance()->play(SoundEffect::FocusLimit);
    return (*m_buttons)[current];
}

brls::View* ResultButtonGrid::create() { return new ResultButtonGrid(); }

// ── SearchActivity ───────────────────────────────────────────

static constexpr float BUTTON_WIDTH   = 388.0f;
static constexpr float BUTTON_SPACING = 18.0f;
static constexpr int   BUTTONS_PER_ROW = 3;

SearchActivity::SearchActivity(const std::vector<std::string>& items,
                               std::function<void(int)> onSelect)
    : m_items(items), m_onSelect(std::move(onSelect))
{
}

void SearchActivity::onContentAvailable() {
    m_frame->setTitle(brls::getStr("search/title"));

    // RT 键 = 切换区域（初始禁用，有结果时启用）
    m_frame->registerAction(brls::getStr("search/switchResult"), brls::ControllerButton::BUTTON_RT, [this](brls::View*) {
        Audio::instance()->play(SoundEffect::Click);
        if (isFocusInResults()) switchToKeyboard();
        else switchToResults();
        return true;
    });

    m_frame->setActionAvailable(brls::ControllerButton::BUTTON_RT, false);

    // X 键 = 返回
    m_frame->registerAction(brls::getStr("search/back"), brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        Audio::instance()->play(SoundEffect::Enter);
        brls::Application::popActivity();
        return true;
    });

    // B 键 = 吞掉事件（防止结果区触发框架默认的"返回"，键盘区的 B 删除优先级更高不受影响）
    m_frame->registerAction("", brls::ControllerButton::BUTTON_B, [](brls::View*) {
        return true;
    }, true);

    // 文字变化事件 → 触发搜索 + 更新计数器
    int maxLen = m_keyboard->getMaxLength();
    m_frame->setIndexText("0 / " + std::to_string(maxLen));
    m_keyboard->getTextChangeEvent()->subscribe([this, maxLen](const std::string& text) {
        updateResults(text);
        m_frame->setIndexText(std::to_string(text.size()) + " / " + std::to_string(maxLen));
    });
}

void SearchActivity::showHint(const std::string& text) {
    m_hint->setText(text);
    m_hint->setVisibility(brls::Visibility::VISIBLE);
    m_buttonContainer->setVisibility(brls::Visibility::GONE);
    m_frame->setActionAvailable(brls::ControllerButton::BUTTON_RT, false);
}

bool SearchActivity::isFocusInResults() {
    auto* view = brls::Application::getCurrentFocus();
    while (view) {
        if (view == m_buttonContainer) return true;
        view = view->getParent();
    }
    return false;
}

void SearchActivity::switchToResults() {
    if (m_resultButtons.empty()) return;
    m_lastKeyboardFocus = brls::Application::getCurrentFocus();
    brls::Application::giveFocus(m_resultButtons[0]);
    m_frame->updateActionHint(brls::ControllerButton::BUTTON_RT, brls::getStr("search/switchKeyboard"));
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void SearchActivity::switchToKeyboard() {
    auto* target = m_lastKeyboardFocus ? m_lastKeyboardFocus : m_keyboard->getDefaultFocus();
    if (target)
        brls::Application::giveFocus(target);
    m_frame->updateActionHint(brls::ControllerButton::BUTTON_RT, brls::getStr("search/switchResult"));
    brls::Application::getGlobalHintsUpdateEvent()->fire();
}

void SearchActivity::updateResults(const std::string& keyword) {
    using brls::Visibility;

    // 安全转移焦点：如果当前焦点在结果区，先转回键盘再销毁
    if (isFocusInResults()) switchToKeyboard();

    m_resultButtons.clear();
    m_buttonContainer->clearViews();

    // 空输入 → 显示空闲提示
    if (keyword.empty()) {
        showHint(brls::getStr("search/hintIdle"));
        return;
    }

    auto results = m_searchEngine.search(keyword, m_items, 6);

    // 有输入无结果 → 显示无结果提示
    if (results.empty()) {
        showHint(brls::getStr("search/hintEmpty"));
        return;
    }

    // 有结果 → 隐藏提示，填充按钮，启用 RT
    m_hint->setVisibility(Visibility::GONE);
    m_buttonContainer->setVisibility(Visibility::VISIBLE);
    m_frame->setActionAvailable(brls::ControllerButton::BUTTON_RT, true);

    // 按行分组填充
    brls::Box* currentRow = nullptr;
    for (size_t i = 0; i < results.size(); i++) {
        if (i % BUTTONS_PER_ROW == 0) {
            currentRow = new ResultPassthroughRow();
            currentRow->setAxis(brls::Axis::ROW);
            currentRow->setJustifyContent(brls::JustifyContent::FLEX_START);
            if (i > 0) currentRow->setMarginTop(BUTTON_SPACING);
            m_buttonContainer->addView(currentRow);
        }
        auto* btn = createResultButton(results[i]);
        if (i % BUTTONS_PER_ROW > 0) btn->setMarginLeft(BUTTON_SPACING);
        currentRow->addView(btn);
        m_resultButtons.push_back(btn);
    }

    // 导航由 ResultButtonGrid::getNextFocus 统一处理
    m_buttonContainer->setButtons(&m_resultButtons);
}

ResultButton* SearchActivity::createResultButton(const SearchEngine::Result& result) {
    auto theme = brls::Application::getTheme();

    auto* btn = new ResultButton();
    btn->setFocusable(true);
    btn->setWidth(BUTTON_WIDTH);
    btn->setAlignItems(brls::AlignItems::CENTER);
    btn->setJustifyContent(brls::JustifyContent::CENTER);
    btn->setPadding(20, 30, 20, 30);
    btn->setBackgroundColor(theme.getColor("app/cardBg"));
    btn->setCornerRadius(8);
    btn->setHighlightCornerRadius(8);
    btn->setShadowType(brls::ShadowType::GENERIC);

    auto* label = new brls::Label();
    label->setSingleLine(true);
    label->setFontSize(23);
    label->setText(result.name);
    btn->addView(label);

    int index = result.index;

    btn->registerAction(brls::getStr("search/confirm"), brls::BUTTON_A, [this, index](...) {
        Audio::instance()->play(SoundEffect::Enter);
        // 拷贝回调，因为 popActivity 后 this 已销毁
        auto callback = m_onSelect;
        brls::Application::popActivity(brls::TransitionAnimation::NONE, [callback, index]() {
            if (callback) callback(index);  // 关闭搜索页后，通知调用方选中了哪项
        });
        return true;
    }, false, false);
    
    btn->addGestureRecognizer(new brls::TapGestureRecognizer(btn));

    return btn;
}
