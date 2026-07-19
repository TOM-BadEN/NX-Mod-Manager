/**
 * Keyboard - QWERTY 全键盘组件
 * 包含输入显示区 + 4 行按键 + 返回/删除按钮
 * 内部管理输入文字和光标，重写 getNextFocus 实现跨行空间导航
 */

#pragma once

#include <borealis.hpp>
#include <functional>
#include <vector>
#include <string>

class KeyButton : public brls::Box {
public:
    /**
     * @brief 构造按键
     * @param character 对应字符
     */
    KeyButton(char character);

    void onFocusGained() override;
    void onFocusLost() override;

    /** @brief 获取对应字符 */
    char getCharacter() const { return m_character; }

private:
    char m_character;
    brls::Label* m_label;
};

class Keyboard : public brls::Box {
public:
    Keyboard();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    /** @brief 获取输入文字 */
    const std::string& getInputText() const { return m_inputText; }

    /** @brief 设置占位提示词 */
    void setPlaceholder(std::string text);

    /** @brief 设置最大输入长度，默认 50 */
    void setMaxLength(int maxLength);

    /** @brief 获取最大输入长度 */
    int getMaxLength() const { return m_maxLength; }

    /** @brief 获取文本变化事件 */
    brls::Event<const std::string&>* getTextChangeEvent() { return &m_textChangeEvent; }

    static brls::View* create();

private:
    // 输入状态
    std::string m_inputText;
    std::string m_placeholder;
    int m_maxLength = 50;
    int m_cursorPosition = 0;
    brls::Label* m_inputLabel = nullptr;

    // 每行的 KeyButton 列表（用于导航查找）
    std::vector<std::vector<KeyButton*>> m_keyButtons;

    // 触摸按钮
    brls::Box* m_spaceButton = nullptr;
    brls::Box* m_deleteButton = nullptr;

    // 触摸删除长按连发（模拟控制器 250ms 首次延迟 + 100ms 连发）
    brls::Timer m_deleteDelayTimer;
    brls::RepeatingTimer m_deleteRepeatTimer;

    // 手柄按键联动触摸按钮点击动画
    bool m_bHolding = false;
    bool m_yHolding = false;

    // 事件
    brls::Event<const std::string&> m_textChangeEvent;

    /** @brief 构建键盘布局 */
    void buildLayout();

    /**
     * @brief 在光标处插入字符
     * @param character 字符
     */
    bool insertChar(char character);

    /** @brief 删除光标前的字符 */
    bool deleteChar();

    /** @brief 光标左移 */
    bool cursorLeft();

    /** @brief 光标右移 */
    bool cursorRight();

    /** @brief 刷新输入区文本和光标 */
    void updateInputDisplay();

    /** @brief 刷新显示 + 触发文本变化事件 */
    void onTextChanged();

    /**
     * @brief 查找最近的按键
     * @param targetRow 目标行
     * @param xCenter x 中心坐标
     */
    KeyButton* findNearestKey(int targetRow, float xCenter);

    /**
     * @brief 查找 View 所在行
     * @param view 目标 View
     */
    int findRowOfView(brls::View* view);
};
