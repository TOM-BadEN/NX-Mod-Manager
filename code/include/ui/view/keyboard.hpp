/**
 * Keyboard - QWERTY 全键盘组件
 *
 * 包含输入显示区、四行按键和删除按钮，内部管理输入文字、
 * 光标与跨行焦点导航。
 */

#pragma once

#include <borealis.hpp>
#include <string>
#include <vector>

/** @brief 键盘中的单个字符按键 */
class KeyButton : public brls::Box {
public:
    /**
     * @brief 创建字符按键
     * @param character 按键对应的字符
     */
    KeyButton(char character);

    /** @brief 获得焦点时恢复高亮和点击动画 */
    void onFocusGained() override;

    /** @brief 失去焦点时清理动画状态 */
    void onFocusLost() override;

    /** @brief 获取按键对应的字符 */
    char getCharacter() const { return m_character; }

private:
    char m_character;            // 按键对应的字符
    brls::Label* m_label = nullptr; // 按键文字
};

/** @brief QWERTY 虚拟键盘 */
class Keyboard : public brls::Box {
public:
    /** @brief 创建并初始化虚拟键盘 */
    Keyboard();

    /**
     * @brief 绘制键盘并处理手柄按键释放动画
     * @param vg NanoVG 绘制上下文
     * @param x 视图横坐标
     * @param y 视图纵坐标
     * @param width 视图宽度
     * @param height 视图高度
     * @param style 当前样式
     * @param ctx 当前帧上下文
     */
    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;

    /**
     * @brief 根据方向查找下一个键盘焦点
     * @param direction 焦点移动方向
     * @param currentView 当前参与导航的 View
     * @return 下一个焦点 View
     */
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    /** @brief 获取当前输入文字 */
    const std::string& getInputText() const { return m_inputText; }

    /**
     * @brief 设置输入框占位文字
     * @param text 占位文字
     */
    void setPlaceholder(std::string text);

    /**
     * @brief 设置最大输入字符数
     * @param maxLength 最大字符数
     */
    void setMaxLength(int maxLength);

    /** @brief 获取最大输入字符数 */
    int getMaxLength() const { return m_maxLength; }

    /** @brief 获取文字变化事件 */
    brls::Event<const std::string&>* getTextChangeEvent() { return &m_textChangeEvent; }

    /** @brief XML View 工厂函数 */
    static brls::View* create();

private:
    std::string m_inputText;                              // 当前输入文字
    std::string m_placeholder;                            // 输入框占位文字
    int m_maxLength = 50;                                 // 最大输入字符数
    int m_cursorPosition = 0;                             // UTF-8 字节光标位置
    brls::Label* m_inputLabel = nullptr;                  // 输入文字显示区域
    std::vector<std::vector<KeyButton*>> m_keyButtons;    // 各行字符按键
    brls::Box* m_spaceButton = nullptr;                   // 空格触摸按钮
    brls::Box* m_deleteButton = nullptr;                  // 删除触摸按钮
    brls::Timer m_deleteDelayTimer;                       // 触摸删除首次延迟计时器
    brls::RepeatingTimer m_deleteRepeatTimer;             // 触摸删除连发计时器
    bool m_bHolding = false;                              // B 键是否处于按下状态
    bool m_yHolding = false;                              // Y 键是否处于按下状态
    brls::Event<const std::string&> m_textChangeEvent;    // 文字变化事件

    /** @brief 构建输入区域和全部键盘按键 */
    void buildLayout();

    /**
     * @brief 在光标位置插入字符
     * @param character 要插入的字符
     * @return 插入成功时返回 true
     */
    bool insertChar(char character);

    /** @brief 删除光标前的一个 UTF-8 字符 */
    bool deleteChar();

    /** @brief 将光标向左移动一个 UTF-8 字符 */
    bool cursorLeft();

    /** @brief 将光标向右移动一个 UTF-8 字符 */
    bool cursorRight();

    /** @brief 刷新输入文字和光标显示 */
    void updateInputDisplay();

    /** @brief 刷新显示并触发文字变化事件 */
    void onTextChanged();

    /**
     * @brief 查找目标行中横向位置最接近的按键
     * @param targetRow 目标行索引
     * @param xCenter 当前焦点中心横坐标
     * @return 找到的字符按键，无有效目标时返回 nullptr
     */
    KeyButton* findNearestKey(int targetRow, float xCenter);

    /**
     * @brief 查找 View 所在的字符按键行
     * @param view 目标 View
     * @return 所在行索引，不属于字符按键时返回 -1
     */
    int findRowOfView(brls::View* view);
};
