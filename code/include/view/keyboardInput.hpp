/**
 * KeyboardInput - 底部键盘输入
 * 半透明遗罩覆盖当前页面，键盘面板贴底部显示
 * + 键确认，X 键返回
 */

#pragma once

#include <borealis.hpp>
#include <functional>
#include <string>
#include "view/keyboard.hpp"

class KeyboardInput : public brls::Box {
public:
    /**
     * @brief 构造键盘输入
     * @param onConfirm 确认回调，参数为用户输入文本
     */
    KeyboardInput(std::function<void(std::string)> onConfirm, std::string placeholder = "", int maxLength = 50);

    bool isTranslucent() override { return true; }

    /**
     * @brief 显示键盘输入
     * @param onConfirm 确认回调
     */
    static void show(std::function<void(std::string)> onConfirm, std::string placeholder = "", int maxLength = 50);

    /** @brief 关闭键盘输入 */
    void closeInput();

private:
    BRLS_BIND(brls::Box, m_panel, "keyboardInput/panel");
    BRLS_BIND(brls::Box, m_content, "keyboardInput/content");
    BRLS_BIND(brls::Label, m_counter, "keyboardInput/counter");

    Keyboard* m_keyboard = nullptr;
};
