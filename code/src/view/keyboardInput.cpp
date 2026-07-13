/**
 * KeyboardInput - 底部键盘输入实现
 */

#include "view/keyboardInput.hpp"
#include "utils/pageNav.hpp"
#include "core/audio.hpp"
#include <borealis/core/i18n.hpp>

static int utf8CharCount(const std::string& s) {
    int count = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) i += 1;
        else if ((c & 0xE0) == 0xC0) i += 2;
        else if ((c & 0xF0) == 0xE0) i += 3;
        else i += 4;
        count++;
    }
    return count;
}

KeyboardInput::KeyboardInput(std::function<void(std::string)> onConfirm, std::string placeholder, int maxLength) {
    inflateFromXMLRes("xml/view/keyboardInput.xml");

    m_keyboard = new Keyboard();
    if (!placeholder.empty()) m_keyboard->setPlaceholder(std::move(placeholder));
    if (maxLength > 0) {
        m_keyboard->setMaxLength(maxLength);
        m_counter->setText("0 / " + std::to_string(maxLength));
        m_keyboard->getTextChangeEvent()->subscribe([this, maxLength](const std::string& text) {
            m_counter->setText(std::to_string(utf8CharCount(text)) + " / " + std::to_string(maxLength));
        });
    }
    m_content->addView(m_keyboard);

    // +：确认输入
    m_keyboard->registerAction(brls::getStr("views/keyboardInput/confirm"), brls::BUTTON_START,
        [this, onConfirm](brls::View*) mutable {
            Audio::instance()->play(SoundEffect::Enter);
            std::string text = m_keyboard->getInputText();
            closeInput();
            if (onConfirm) onConfirm(std::move(text));
            return true;
        });

    // X：返回
    m_keyboard->registerAction(brls::getStr("views/keyboardInput/back"), brls::BUTTON_X,
        [this](brls::View*) {
            Audio::instance()->play(SoundEffect::Enter);
            closeInput();
            return true;
        });
}

void KeyboardInput::show(std::function<void(std::string)> onConfirm, std::string placeholder, int maxLength) {
    auto* input = new KeyboardInput(std::move(onConfirm), std::move(placeholder), maxLength);
    pageNav::push(new brls::Activity(input));
}

void KeyboardInput::closeInput() {
    brls::Application::popActivity(brls::TransitionAnimation::NONE);
}
