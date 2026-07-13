/**
 * Keyboard - QWERTY 全键盘组件
 * 包含输入显示区 + 4 行按键 + 返回/删除按钮
 * 内部管理输入文字和光标，重写 getNextFocus 实现跨行空间导航
 */

#include "view/keyboard.hpp"
#include <borealis/core/i18n.hpp>
#include "core/audio.hpp"
#include <limits>
#include <algorithm>
#include <yoga/Yoga.h>

// ── UTF-8 工具函数 ───────────────────────────────────────────────

static int utf8CharLen(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static int utf8PrevPos(const std::string& s, int pos) {
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) pos--;
    return pos;
}

static int utf8NextPos(const std::string& s, int pos) {
    if (pos >= static_cast<int>(s.size())) return static_cast<int>(s.size());
    return pos + utf8CharLen(static_cast<unsigned char>(s[pos]));
}

static int utf8ByteToCharIndex(const std::string& s, int bytePos) {
    int charIdx = 0, i = 0;
    while (i < bytePos && i < static_cast<int>(s.size())) {
        i += utf8CharLen(static_cast<unsigned char>(s[i]));
        charIdx++;
    }
    return charIdx;
}

static int utf8CharCount(const std::string& s) {
    return utf8ByteToCharIndex(s, static_cast<int>(s.size()));
}

// ── QWERTY 键盘布局定义 ─────────────────────────────────────────

static const std::vector<std::string> KEYBOARD_ROWS = {
    "1234567890",
    "QWERTYUIOP",
    "ASDFGHJKL",
    "ZXCVBNM",
};

// 按钮通用样式（KeyButton 和触摸按钮共用）
static void applyButtonStyle(brls::Box* button) {
    button->setAlignItems(brls::AlignItems::CENTER);
    button->setJustifyContent(brls::JustifyContent::CENTER);
    button->setCornerRadius(4);
    button->setBackgroundColor(brls::Application::getTheme().getColor("app/cardBg"));
    button->setShadowType(brls::ShadowType::GENERIC);
}

// 创建绝对定位的按键提示图标
static brls::Label* createHintLabel(const std::string& icon) {
    auto* label = new brls::Label();
    label->setText(icon);
    label->setFontSize(16);
    YGNodeStyleSetPositionType(label->getYGNode(), YGPositionTypeAbsolute);
    YGNodeStyleSetPosition(label->getYGNode(), YGEdgeTop, 10);
    YGNodeStyleSetPosition(label->getYGNode(), YGEdgeRight, 8);
    return label;
}

// ── KeyButton ───────────────────────────────────────────────────

KeyButton::KeyButton(char character) : m_character(character) {
    setFocusable(true);
    applyButtonStyle(this);

    m_label = new brls::Label();
    m_label->setText(std::string(1, character));
    m_label->setFontSize(24);
    addView(m_label);

    // 点击时由 Keyboard 统一处理事件（通过 registerAction）
}

void KeyButton::onFocusGained() {
    setHideHighlight(false);
    setHideClickAnimation(false);
    Box::onFocusGained();
}

void KeyButton::onFocusLost() {
    setHideHighlight(true);
    // 清除点击脉冲：playClickAnimation(false) 内部 clickAlpha.reset(0)，随即 stop 保持归零
    playClickAnimation(false, false, true);
    resetClickAnimation();
    setHideClickAnimation(true);
    Box::onFocusLost(); 
}

// 行容器：行内 LEFT/RIGHT 导航成功时播放 Focus 音效
class KeyboardRowBox : public brls::Box {
public:
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override {
        auto* next = Box::getNextFocus(direction, currentView);
        if (next) Audio::instance()->play(SoundEffect::Focus);
        return next;
    }
};

// UP/DOWN 透传容器：不拦截纵向导航，委托给父级处理
class PassthroughColumn : public brls::Box {
public:
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override {
        if (direction == brls::FocusDirection::UP || direction == brls::FocusDirection::DOWN) {
            if (hasParent()) return getParent()->getNextFocus(direction, this);
            return nullptr;
        }
        return Box::getNextFocus(direction, currentView);
    }
};

// ── Keyboard ────────────────────────────────────────────────────

Keyboard::Keyboard() {
    setAxis(brls::Axis::COLUMN);
    setJustifyContent(brls::JustifyContent::FLEX_START);
    setAlignItems(brls::AlignItems::CENTER);

    m_placeholder = brls::getStr("views/keyboard/placeholder");
    
    buildLayout();

    // -：打开系统原生键盘，支持非拉丁文字输入
    registerAction(brls::getStr("views/keyboard/nativeKeyboard"), brls::ControllerButton::BUTTON_BACK, [this](brls::View*) {
        auto onResult = [this](std::string result) {
            m_inputText = result;
            m_cursorPosition = static_cast<int>(m_inputText.size());
            onTextChanged();
        };
        Audio::instance()->play(SoundEffect::Enter);
        brls::Application::getPlatform()->getImeManager()->openForText(onResult, brls::getStr("views/keyboard/search"), "", 50, m_inputText);
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
    registerAction(brls::getStr("views/keyboard/space"), brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
        Audio::instance()->play(insertChar(' ') ? SoundEffect::Focus : SoundEffect::FocusLimit);
        if (!m_yHolding) {
            // 联动空格按钮的按下动画
            m_spaceButton->playClickAnimation(false, false); 
            m_yHolding = true;
        }
        return true;
    }, true);

    // B：删除光标前字符，同步触发触摸删除按钮的点击动画
    registerAction(brls::getStr("views/keyboard/delete"), brls::ControllerButton::BUTTON_B, [this](brls::View*) {
        Audio::instance()->play(deleteChar() ? SoundEffect::Focus : SoundEffect::FocusLimit);
        if (!m_bHolding) {
            // 联动删除按钮的按下动画
            m_deleteButton->playClickAnimation(false, false);
            m_bHolding = true;
        }
        return true;
    }, true, true);
}

void Keyboard::buildLayout() {
    const float keyWidth = 108.0f;   // 按键宽度
    const float keyHeight = 64.0f;   // 按键高度
    const float keySpacing = 8.0f;   // 按键水平间距
    const float rowSpacing = 10.0f;  // 行间距

    m_keyButtons.resize(KEYBOARD_ROWS.size());

    // 输入显示区
    m_inputLabel = new brls::Label();
    m_inputLabel->setText(brls::getStr("views/keyboard/placeholder"));
    m_inputLabel->setFontSize(24);
    m_inputLabel->setVerticalAlign(brls::VerticalAlign::TOP);
    m_inputLabel->setMarginBottom(16);
    addView(m_inputLabel);

    // 创建单行按键 Row Box 的辅助 lambda
    auto createRow = [&](size_t rowIndex) {
        const std::string& rowChars = KEYBOARD_ROWS[rowIndex];
        auto* rowBox = new KeyboardRowBox();
        rowBox->setAxis(brls::Axis::ROW);
        rowBox->setJustifyContent(brls::JustifyContent::FLEX_START);
        rowBox->setAlignItems(brls::AlignItems::CENTER);

        for (size_t colIndex = 0; colIndex < rowChars.size(); colIndex++) {
            auto* keyButton = new KeyButton(rowChars[colIndex]);
            keyButton->setWidth(keyWidth);
            keyButton->setHeight(keyHeight);
            if (colIndex > 0) keyButton->setMarginLeft(keySpacing);

            keyButton->registerAction(brls::getStr("views/keyboard/input"), brls::BUTTON_A, [this, keyButton](...) {
                Audio::instance()->play(insertChar(keyButton->getCharacter()) ? SoundEffect::Focus : SoundEffect::FocusLimit);
                return true;
            }, false, false);
            
            // 触摸点击支持
            keyButton->addGestureRecognizer(new brls::TapGestureRecognizer(keyButton)); 

            rowBox->addView(keyButton);
            m_keyButtons[rowIndex].push_back(keyButton);
        }
        return rowBox;
    };

    // 行 0、1 直接加入 Keyboard
    addView(createRow(0));
    auto* row1 = createRow(1);
    row1->setMarginTop(rowSpacing);
    addView(row1);

    // 行 2、3 + 返回/删除按钮 → BottomSection
    auto* bottomSection = new brls::Box();
    bottomSection->setAxis(brls::Axis::ROW);
    bottomSection->setAlignItems(brls::AlignItems::FLEX_START);
    bottomSection->setMarginTop(rowSpacing);

    // 中间列：行 2 + 行 3（含空格按钮）
    auto* middleColumn = new PassthroughColumn();
    middleColumn->setAxis(brls::Axis::COLUMN);

    auto* row2 = createRow(2);
    middleColumn->addView(row2);

    auto* row3 = createRow(3);
    row3->setMarginTop(rowSpacing);

    // 空格按钮：行 3 末尾，横向占 2 格
    m_spaceButton = new brls::Box();
    applyButtonStyle(m_spaceButton);
    m_spaceButton->setWidth(keyWidth * 2 + keySpacing);
    m_spaceButton->setHeight(keyHeight);
    m_spaceButton->setMarginLeft(keySpacing);
    auto* spaceLabel = new brls::Label();
    spaceLabel->setText(brls::getStr("views/keyboard/space"));
    spaceLabel->setFontSize(22);
    m_spaceButton->addView(spaceLabel);
    m_spaceButton->addView(createHintLabel("\uE0E3"));
    m_spaceButton->addGestureRecognizer(new brls::TapGestureRecognizer(m_spaceButton, [this]() {
        Audio::instance()->play(insertChar(' ') ? SoundEffect::Focus : SoundEffect::FocusLimit);
    }));
    row3->addView(m_spaceButton);

    middleColumn->addView(row3);

    // 删除按钮：行 2-3 最右列，纵向占 2 格
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
    // 触摸长按连发：UNSURE=按下 → 立即删除+启动延迟定时器；END/FAILED=抬起 → 停止
    m_deleteRepeatTimer.setCallback([this]() {
        Audio::instance()->play(deleteChar() ? SoundEffect::Focus : SoundEffect::FocusLimit);
    });
    m_deleteRepeatTimer.setPeriod(100);
    m_deleteDelayTimer.setDuration(250);
    m_deleteDelayTimer.setEndCallback([this](bool) { m_deleteRepeatTimer.start(); });

    m_deleteButton->addGestureRecognizer(
        new brls::TapGestureRecognizer([this](brls::TapGestureStatus status, brls::Sound* sound) {
            m_deleteButton->playClickAnimation(status.state != brls::GestureState::UNSURE);

            if (status.state == brls::GestureState::UNSURE) {
                if (!m_deleteDelayTimer.isRunning() && !m_deleteRepeatTimer.isRunning()) {
                    Audio::instance()->play(deleteChar() ? SoundEffect::Focus : SoundEffect::FocusLimit);
                    m_deleteDelayTimer.start();
                }
            } else if (status.state == brls::GestureState::END || status.state == brls::GestureState::FAILED) {
                m_deleteDelayTimer.stop();
                m_deleteRepeatTimer.stop();
            }
        }));

    // 组装：中间列 + 删除按钮
    bottomSection->addView(middleColumn);
    bottomSection->addView(m_deleteButton);

    addView(bottomSection);
}

// ── 输入操作 ────────────────────────────────────────────────────

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
    int prev = utf8PrevPos(m_inputText, m_cursorPosition);
    m_inputText.erase(prev, m_cursorPosition - prev);
    m_cursorPosition = prev;
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

// ── 导航逻辑 ────────────────────────────────────────────────────

int Keyboard::findRowOfView(brls::View* view) {
    for (size_t row = 0; row < m_keyButtons.size(); row++) {
        for (auto* key : m_keyButtons[row]) {
            if (key == view) return static_cast<int>(row);
        }
    }
    return -1;
}

KeyButton* Keyboard::findNearestKey(int targetRow, float xCenter) {
    if (targetRow < 0 || targetRow >= static_cast<int>(m_keyButtons.size())) return nullptr;

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

void Keyboard::draw(NVGcontext* vg, float x, float y, float width, float height,
                    brls::Style style, brls::FrameContext* ctx) {
    // 手柄按键松开 → 触摸按钮反向脉冲
    auto& buttons = brls::Application::getControllerState().buttons;
    if (m_bHolding && !buttons[brls::BUTTON_B]) {
        m_deleteButton->playClickAnimation(true);
        m_bHolding = false;
    }
    if (m_yHolding && !buttons[brls::BUTTON_Y]) {
        m_spaceButton->playClickAnimation(true);
        m_yHolding = false;
    }
    Box::draw(vg, x, y, width, height, style, ctx);
}

brls::View* Keyboard::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    brls::View* focused = brls::Application::getCurrentFocus();
    int currentRow = findRowOfView(focused);

    if (currentRow < 0) return Box::getNextFocus(direction, currentView);

    float focusCenterX = focused->getX() + focused->getWidth() / 2.0f;

    // UP/DOWN：跨行空间匹配（循环）
    if (direction == brls::FocusDirection::UP) {
        int targetRow = (currentRow == 0) ? static_cast<int>(m_keyButtons.size()) - 1 : currentRow - 1;
        KeyButton* target = findNearestKey(targetRow, focusCenterX);
        if (target) { Audio::instance()->play(SoundEffect::Focus); return target; }
    }
    if (direction == brls::FocusDirection::DOWN) {
        // 行 3 按 DOWN → 回到行 0（循环）
        int targetRow = (currentRow >= static_cast<int>(m_keyButtons.size()) - 1) ? 0 : currentRow + 1;
        KeyButton* target = findNearestKey(targetRow, focusCenterX);
        if (target) { Audio::instance()->play(SoundEffect::Focus); return target; }
    }

    // LEFT/RIGHT：行首行尾循环
    if (direction == brls::FocusDirection::LEFT && focused == m_keyButtons[currentRow].front()) {
        Audio::instance()->play(SoundEffect::Focus);
        return m_keyButtons[currentRow].back();
    }
    if (direction == brls::FocusDirection::RIGHT && focused == m_keyButtons[currentRow].back()) {
        Audio::instance()->play(SoundEffect::Focus);
        return m_keyButtons[currentRow].front();
    }

    auto* next = Box::getNextFocus(direction, currentView);
    if (next) Audio::instance()->play(SoundEffect::Focus);
    return next;
}

brls::View* Keyboard::create() {
    return new Keyboard();
}
