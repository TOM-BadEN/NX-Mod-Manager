/**
 * Keyboard - QWERTY 全键盘组件
 */

#include "ui/view/keyboard.hpp"
#include "core/audio.hpp"
#include <algorithm>
#include <borealis/core/i18n.hpp>
#include <limits>
#include <utility>
#include <yoga/Yoga.h>

namespace {

int utf8CharLen(unsigned char character) {
    if (character < 0x80) return 1;
    if ((character & 0xE0) == 0xC0) return 2;
    if ((character & 0xF0) == 0xE0) return 3;
    if ((character & 0xF8) == 0xF0) return 4;
    return 1;
}

int utf8PrevPos(const std::string& text, int position) {
    if (position <= 0) return 0;

    position--;
    while (position > 0
        && (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) {
        position--;
    }
    return position;
}

int utf8NextPos(const std::string& text, int position) {
    if (position >= static_cast<int>(text.size())) return static_cast<int>(text.size());
    return position + utf8CharLen(static_cast<unsigned char>(text[position]));
}

int utf8ByteToCharIndex(const std::string& text, int bytePosition) {
    int charIndex = 0;
    int position = 0;
    while (position < bytePosition && position < static_cast<int>(text.size())) {
        position += utf8CharLen(static_cast<unsigned char>(text[position]));
        charIndex++;
    }
    return charIndex;
}

int utf8CharCount(const std::string& text) {
    return utf8ByteToCharIndex(text, static_cast<int>(text.size()));
}

const std::vector<std::string> KEYBOARD_ROWS = {
    "1234567890",
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM",
};

void applyButtonStyle(brls::Box* button) {
    button->setAlignItems(brls::AlignItems::CENTER);
    button->setJustifyContent(brls::JustifyContent::CENTER);
    button->setCornerRadius(4);
    button->setBackgroundColor(brls::Application::getTheme().getColor("app/cardBg"));
    button->setShadowType(brls::ShadowType::GENERIC);
}

brls::Label* createHintLabel(const std::string& icon) {
    auto* label = new brls::Label();
    label->setText(icon);
    label->setFontSize(16);
    YGNodeStyleSetPositionType(label->getYGNode(), YGPositionTypeAbsolute);
    YGNodeStyleSetPosition(label->getYGNode(), YGEdgeTop, 10);
    YGNodeStyleSetPosition(label->getYGNode(), YGEdgeRight, 8);
    return label;
}

/** @brief 为键盘单行提供左右焦点导航音效 */
class KeyboardRowBox : public brls::Box {
public:
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override
    {
        auto* next = brls::Box::getNextFocus(direction, currentView);
        if (next) Audio::instance()->play(SoundEffect::Focus);
        return next;
    }
};

/** @brief 将上下焦点导航交给 Keyboard 统一处理 */
class PassthroughColumn : public brls::Box {
public:
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override
    {
        if (direction == brls::FocusDirection::UP
            || direction == brls::FocusDirection::DOWN) {
            if (hasParent()) return getParent()->getNextFocus(direction, this);
            return nullptr;
        }
        return brls::Box::getNextFocus(direction, currentView);
    }
};

} // namespace

KeyButton::KeyButton(char character)
    : m_character(character) {
    setFocusable(true);
    applyButtonStyle(this);

    m_label = new brls::Label();
    m_label->setText(std::string(1, character));
    m_label->setFontSize(24);
    addView(m_label);
}

void KeyButton::onFocusGained() {
    setHideHighlight(false);
    setHideClickAnimation(false);
    brls::Box::onFocusGained();
}

void KeyButton::onFocusLost() {
    setHideHighlight(true);
    playClickAnimation(false, false, true);
    resetClickAnimation();
    setHideClickAnimation(true);
    brls::Box::onFocusLost();
}

Keyboard::Keyboard() {
    setAxis(brls::Axis::COLUMN);
    setJustifyContent(brls::JustifyContent::FLEX_START);
    setAlignItems(brls::AlignItems::CENTER);

    m_placeholder = brls::getStr("view/keyboard/placeholder");

    buildLayout();

    // -：打开系统原生键盘，支持非拉丁文字输入
    registerAction(brls::getStr("view/keyboard/nativeKeyboard"), brls::ControllerButton::BUTTON_BACK, [this](brls::View*) {
        auto onResult = [this](std::string result) {
            m_inputText = result;
            m_cursorPosition = static_cast<int>(m_inputText.size());
            onTextChanged();
        };
        Audio::instance()->play(SoundEffect::Enter);
        brls::Application::getPlatform()->getImeManager()->openForText(onResult, brls::getStr("view/keyboard/search"), "", 50, m_inputText);
        return true;
    });

    // LB：光标左移
    registerAction("\uE091", brls::ControllerButton::BUTTON_LB, [this](brls::View*) {
        Audio::instance()->play(cursorLeft() ? SoundEffect::Focus : SoundEffect::FocusLimit);
        return true;
    }, false, true);

    // RB：光标右移
    registerAction("\uE090", brls::ControllerButton::BUTTON_RB, [this](brls::View*) {
        Audio::instance()->play(cursorRight() ? SoundEffect::Focus : SoundEffect::FocusLimit);
        return true;
    }, false, true);

    // Y：插入空格，同步触发触摸空格按钮的点击动画
    registerAction(brls::getStr("view/keyboard/space"), brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
        Audio::instance()->play(insertChar(' ') ? SoundEffect::Focus : SoundEffect::FocusLimit);
        if (!m_yHolding) {
            m_spaceButton->playClickAnimation(false, false);
            m_yHolding = true;
        }
        return true;
    }, true);

    // B：删除光标前字符，同步触发触摸删除按钮的点击动画
    registerAction(brls::getStr("view/keyboard/delete"), brls::ControllerButton::BUTTON_B, [this](brls::View*) {
        Audio::instance()->play(deleteChar() ? SoundEffect::Focus : SoundEffect::FocusLimit);
        if (!m_bHolding) {
            m_deleteButton->playClickAnimation(false, false);
            m_bHolding = true;
        }
        return true;
    }, true, true);
}

void Keyboard::buildLayout() {
    const float keyWidth = 108.0f;
    const float keyHeight = 64.0f;
    const float keySpacing = 8.0f;
    const float rowSpacing = 10.0f;

    m_keyButtons.resize(KEYBOARD_ROWS.size());

    m_inputLabel = new brls::Label();
    m_inputLabel->setText(brls::getStr("view/keyboard/placeholder"));
    m_inputLabel->setFontSize(24);
    m_inputLabel->setVerticalAlign(brls::VerticalAlign::TOP);
    m_inputLabel->setMarginTop(16);
    m_inputLabel->setMarginBottom(16);
    addView(m_inputLabel);

    auto createRow = [this, keyWidth, keyHeight, keySpacing](size_t rowIndex) {
        const std::string& rowCharacters = KEYBOARD_ROWS[rowIndex];
        auto* rowBox = new KeyboardRowBox();
        rowBox->setAxis(brls::Axis::ROW);
        rowBox->setJustifyContent(brls::JustifyContent::FLEX_START);
        rowBox->setAlignItems(brls::AlignItems::CENTER);

        for (size_t columnIndex = 0; columnIndex < rowCharacters.size(); columnIndex++) {
            auto* keyButton = new KeyButton(rowCharacters[columnIndex]);
            keyButton->setWidth(keyWidth);
            keyButton->setHeight(keyHeight);
            if (columnIndex > 0) keyButton->setMarginLeft(keySpacing);

            keyButton->registerAction(brls::getStr("view/keyboard/input"), brls::BUTTON_A, [this, keyButton](...) {
                Audio::instance()->play(insertChar(keyButton->getCharacter()) ? SoundEffect::Focus : SoundEffect::FocusLimit);
                return true;
            }, false, false);

            keyButton->addGestureRecognizer(new brls::TapGestureRecognizer(keyButton));

            rowBox->addView(keyButton);
            m_keyButtons[rowIndex].push_back(keyButton);
        }
        return rowBox;
    };

    addView(createRow(0));

    auto* row1 = createRow(1);
    row1->setMarginTop(rowSpacing);
    addView(row1);

    auto* bottomSection = new brls::Box();
    bottomSection->setAxis(brls::Axis::ROW);
    bottomSection->setAlignItems(brls::AlignItems::FLEX_START);
    bottomSection->setMarginTop(rowSpacing);

    auto* middleColumn = new PassthroughColumn();
    middleColumn->setAxis(brls::Axis::COLUMN);

    auto* row2 = createRow(2);
    middleColumn->addView(row2);

    auto* row3 = createRow(3);
    row3->setMarginTop(rowSpacing);

    m_spaceButton = new brls::Box();
    applyButtonStyle(m_spaceButton);
    m_spaceButton->setWidth(keyWidth * 2 + keySpacing);
    m_spaceButton->setHeight(keyHeight);
    m_spaceButton->setMarginLeft(keySpacing);

    auto* spaceLabel = new brls::Label();
    spaceLabel->setText(brls::getStr("view/keyboard/space"));
    spaceLabel->setFontSize(22);
    m_spaceButton->addView(spaceLabel);
    m_spaceButton->addView(createHintLabel("\uE0E3"));
    m_spaceButton->addGestureRecognizer(new brls::TapGestureRecognizer(m_spaceButton, [this]() {
        Audio::instance()->play(insertChar(' ') ? SoundEffect::Focus : SoundEffect::FocusLimit);
    }));
    row3->addView(m_spaceButton);

    middleColumn->addView(row3);

    m_deleteButton = new brls::Box();
    applyButtonStyle(m_deleteButton);
    m_deleteButton->setWidth(keyWidth);
    m_deleteButton->setHeight(keyHeight * 2 + rowSpacing);
    m_deleteButton->setMarginLeft(keySpacing);

    auto* deleteLabel = new brls::Label();
    deleteLabel->setText("\uE070");
    deleteLabel->setFontSize(22);
    m_deleteButton->addView(deleteLabel);
    m_deleteButton->addView(createHintLabel("\uE0E1"));

    // 触摸长按删除：按下后延迟 250ms，再以 100ms 间隔连发
    m_deleteRepeatTimer.setCallback([this]() {
        Audio::instance()->play(deleteChar() ? SoundEffect::Focus : SoundEffect::FocusLimit);
    });
    m_deleteRepeatTimer.setPeriod(100);
    m_deleteDelayTimer.setDuration(250);
    m_deleteDelayTimer.setEndCallback([this](bool) {
        m_deleteRepeatTimer.start();
    });

    m_deleteButton->addGestureRecognizer(new brls::TapGestureRecognizer([this](brls::TapGestureStatus status, brls::Sound*) {
        m_deleteButton->playClickAnimation(status.state != brls::GestureState::UNSURE);

        if (status.state == brls::GestureState::UNSURE) {
            if (!m_deleteDelayTimer.isRunning()
                && !m_deleteRepeatTimer.isRunning()) {
                Audio::instance()->play(deleteChar() ? SoundEffect::Focus : SoundEffect::FocusLimit);
                m_deleteDelayTimer.start();
            }
        } else if (status.state == brls::GestureState::END
            || status.state == brls::GestureState::FAILED) {
            m_deleteDelayTimer.stop();
            m_deleteRepeatTimer.stop();
        }
    }));

    bottomSection->addView(middleColumn);
    bottomSection->addView(m_deleteButton);
    addView(bottomSection);
}

bool Keyboard::insertChar(char character) {
    if (utf8CharCount(m_inputText) >= m_maxLength) return false;
    if (character == ' ' && m_inputText.empty()) return false;

    m_inputText.insert(m_cursorPosition, 1, character);
    m_cursorPosition++;
    onTextChanged();
    return true;
}

bool Keyboard::deleteChar() {
    if (m_cursorPosition <= 0) return false;

    int previous = utf8PrevPos(m_inputText, m_cursorPosition);
    m_inputText.erase(previous, m_cursorPosition - previous);
    m_cursorPosition = previous;
    onTextChanged();
    return true;
}

bool Keyboard::cursorLeft() {
    if (m_cursorPosition <= 0) return false;

    m_cursorPosition = utf8PrevPos(m_inputText, m_cursorPosition);
    updateInputDisplay();
    return true;
}

bool Keyboard::cursorRight() {
    if (m_cursorPosition >= static_cast<int>(m_inputText.size())) return false;

    m_cursorPosition = utf8NextPos(m_inputText, m_cursorPosition);
    updateInputDisplay();
    return true;
}

void Keyboard::updateInputDisplay() {
    if (m_inputText.empty()) {
        m_inputLabel->setText(m_placeholder);
        m_inputLabel->setCursor(static_cast<int>(brls::CursorPosition::UNSET));
    } else {
        m_inputLabel->setText(m_inputText);
        m_inputLabel->setCursor(utf8ByteToCharIndex(m_inputText, m_cursorPosition));
    }
}

void Keyboard::setMaxLength(int maxLength) {
    m_maxLength = std::min(maxLength, 500);
}

void Keyboard::setPlaceholder(std::string text) {
    m_placeholder = std::move(text);
    if (m_inputText.empty()) m_inputLabel->setText(m_placeholder);
}

void Keyboard::onTextChanged() {
    updateInputDisplay();
    m_textChangeEvent.fire(m_inputText);
}

int Keyboard::findRowOfView(brls::View* view) {
    for (size_t row = 0; row < m_keyButtons.size(); row++) {
        for (auto* key : m_keyButtons[row]) {
            if (key == view) return static_cast<int>(row);
        }
    }
    return -1;
}

KeyButton* Keyboard::findNearestKey(int targetRow, float xCenter) {
    if (targetRow < 0
        || targetRow >= static_cast<int>(m_keyButtons.size())) return nullptr;

    KeyButton* nearest = nullptr;
    float minDistance = std::numeric_limits<float>::max();

    for (auto* key : m_keyButtons[targetRow]) {
        float keyCenterX = key->getX() + key->getWidth() / 2.0f;
        float distance = std::abs(keyCenterX - xCenter);
        if (distance < minDistance) {
            minDistance = distance;
            nearest = key;
        }
    }
    return nearest;
}

void Keyboard::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx)
{
    auto& buttons = brls::Application::getControllerState().buttons;
    if (m_bHolding && !buttons[brls::BUTTON_B]) {
        m_deleteButton->playClickAnimation(true);
        m_bHolding = false;
    }
    if (m_yHolding && !buttons[brls::BUTTON_Y]) {
        m_spaceButton->playClickAnimation(true);
        m_yHolding = false;
    }
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

brls::View* Keyboard::getNextFocus(brls::FocusDirection direction, brls::View* currentView)
{
    brls::View* focused = brls::Application::getCurrentFocus();
    int currentRow = findRowOfView(focused);
    if (currentRow < 0) return brls::Box::getNextFocus(direction, currentView);

    float focusCenterX = focused->getX() + focused->getWidth() / 2.0f;

    if (direction == brls::FocusDirection::UP) {
        int targetRow = currentRow == 0
            ? static_cast<int>(m_keyButtons.size()) - 1
            : currentRow - 1;
        auto* target = findNearestKey(targetRow, focusCenterX);
        if (target) {
            Audio::instance()->play(SoundEffect::Focus);
            return target;
        }
    }

    if (direction == brls::FocusDirection::DOWN) {
        int targetRow = currentRow >= static_cast<int>(m_keyButtons.size()) - 1
            ? 0
            : currentRow + 1;
        auto* target = findNearestKey(targetRow, focusCenterX);
        if (target) {
            Audio::instance()->play(SoundEffect::Focus);
            return target;
        }
    }

    if (direction == brls::FocusDirection::LEFT
        && focused == m_keyButtons[currentRow].front()) {
        Audio::instance()->play(SoundEffect::Focus);
        return m_keyButtons[currentRow].back();
    }

    if (direction == brls::FocusDirection::RIGHT
        && focused == m_keyButtons[currentRow].back()) {
        Audio::instance()->play(SoundEffect::Focus);
        return m_keyButtons[currentRow].front();
    }

    auto* next = brls::Box::getNextFocus(direction, currentView);
    if (next) Audio::instance()->play(SoundEffect::Focus);
    return next;
}

brls::View* Keyboard::create() {
    return new Keyboard();
}
