# Borealis 通知样式自定义（NotificationStyle）

## 目标

让调用者在程序启动时全局设置通知外观，不设置则保持框架默认行为。
向后完全兼容，C++17，适合提 PR。`setStyle` 必须在 `Application::init()` 之前调用。

## 调用示例（用户项目 main.cpp）

```cpp
// C++17
brls::NotificationStyle style;
style.bgColor      = nvgRGBA(45, 45, 45, 200);
style.cornerRadius = 6.0f;
style.gap          = 8.0f;
brls::NotificationManager::setStyle(style);

// C++20（designated initializers）
brls::NotificationManager::setStyle({
    .bgColor       = nvgRGBA(45, 45, 45, 200),
    .cornerRadius  = 6.0f,
    .gap           = 8.0f,
});
```

---

## 修改后完整代码

### notification_manager.hpp

```cpp
/*
Borealis, a Nintendo Switch UI Library
Copyright (C) 2019  natinusala
Copyright (C) 2024  xfangfang

This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <borealis/core/animation.hpp>
#include <borealis/core/box.hpp>
#include <borealis/views/label.hpp>

namespace brls
{

/**
 * 通知全局样式配置。
 * 所有字段默认为 0，表示使用框架 style metric 中的默认值。
 * 在 Application::init() 之前通过 NotificationManager::setStyle() 设置。
 */
struct NotificationStyle {
    // 外观
    NVGcolor bgColor   = {0};  // alpha=0 → 使用默认 BACKDROP；alpha>0 → 纯色背景
    float cornerRadius = 0;    // 仅 bgColor 生效时有效（BACKDROP 不支持圆角）
    NVGcolor textColor = {0};  // alpha=0 → 运行时填充为白色
    float fontSize     = 0;    // 0 → 使用 Label 默认字号

    // 布局
    float width   = 0;  // 0 → 运行时从 style metric 读取
    float padding = 0;  // 0 → 运行时从 style metric 读取
    float gap     = 0;  // 0 → 通知之间无间距

    // 动画
    float timeout      = 0;  // 0 → 运行时从 style metric 读取（停留时长 ms）
    float showDuration = 0;  // 0 → 运行时从 style metric 读取（滑入/滑出时长 ms）
    float slide        = 0;  // 0 → 运行时从 style metric 读取（滑出距离 px）
};

class Notification : public Box
{
  public:
    explicit Notification(const std::string& text);
    ~Notification() override;

    Animatable timeoutTimer;

  private:
    Label* label;
};

class NotificationManager : public Box
{
  public:
    NotificationManager();
    ~NotificationManager() override;

    void notify(const std::string& text);

    /**
     * 设置通知全局样式，必须在 Application::init() 之前调用。
     * 未设置的字段（值为 0）将在初始化时从 style metric 填充默认值。
     */
    static void setStyle(const NotificationStyle& style);

  private:
    static inline NotificationStyle customStyle;

    friend class Notification;
};

}; // namespace brls
```

### notification_manager.cpp

```cpp
/*
Borealis, a Nintendo Switch UI Library
Copyright (C) 2019  natinusala
Copyright (C) 2024  xfangfang

This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
        the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/notification_manager.hpp>

namespace brls
{

// 设置通知全局样式
void NotificationManager::setStyle(const NotificationStyle& style)
{
    customStyle = style;
}

// 通知管理器构造：未被用户设置的字段从 style metric 填入默认值
NotificationManager::NotificationManager()
{
    auto style = Application::getStyle();
    auto& s    = customStyle;

    if (s.width == 0)        s.width        = style.getMetric("brls/notification/width");
    if (s.padding == 0)      s.padding      = style.getMetric("brls/notification/padding");
    if (s.timeout == 0)      s.timeout      = style.getMetric("brls/animations/notification_timeout");
    if (s.showDuration == 0) s.showDuration = style.getMetric("brls/animations/notification_show");
    if (s.slide == 0)        s.slide        = style.getMetric("brls/notification/slide");
    if (s.textColor.a == 0)  s.textColor    = nvgRGBA(255, 255, 255, 255);

    this->setWidth(s.width);
    this->setTranslationX(Application::ORIGINAL_WINDOW_WIDTH - s.width);
    this->setAxis(Axis::COLUMN);
}

// 显示一条通知
void NotificationManager::notify(const std::string& text)
{
    brls::Logger::debug("Showing notification \"{}\"", text);

    auto* notification = new Notification(text);
    this->addView(notification, 0);

    // ── 超时动画：滑入 → 停留 → 滑出 → 移除 ──
    auto& s       = customStyle;
    float timeout = s.timeout;
    float show    = s.showDuration;
    float slide   = s.slide;

    notification->timeoutTimer.reset(slide);
    notification->timeoutTimer.addStep(0.0f, (int)show, EasingFunction::quadraticOut);
    notification->timeoutTimer.addStep(0.0f, (int)timeout, EasingFunction::linear);
    notification->timeoutTimer.addStep(slide, (int)show, EasingFunction::quadraticOut);

    notification->timeoutTimer.setTickCallback([notification, slide]()
        {
            float position = notification->timeoutTimer.getValue();
            notification->setTranslationX(position);
            notification->setAlpha(1.0f - position / slide);
        });

    notification->timeoutTimer.setEndCallback([this, notification](bool finished)
        { this->removeView(notification); });

    notification->timeoutTimer.start();
}

// 析构时停止所有通知的动画
NotificationManager::~NotificationManager()
{
    std::vector<View*> views = this->getChildren();
    for (auto& view : views)
    {
        auto label = dynamic_cast<Notification*>(view);
        label->timeoutTimer.stop();
    }
}

// 单条通知构造：从 customStyle 读取样式
Notification::Notification(const std::string& text)
{
    auto& s = NotificationManager::customStyle;

    // 背景：BACKDROP（毛玻璃）和纯色是两种不同渲染模式
    if (s.bgColor.a > 0)
    {
        this->setBackgroundColor(s.bgColor);
        this->setCornerRadius(s.cornerRadius);
    }
    else
    {
        this->setBackground(ViewBackground::BACKDROP);
    }

    this->setPadding(s.padding);
    this->setWidth(s.width);

    this->label = new Label();
    this->label->setText(text);
    this->label->setTextColor(s.textColor);
    if (s.fontSize > 0) this->label->setFontSize(s.fontSize);
    this->addView(label);

    if (s.gap > 0) this->setMarginBottom(s.gap);
}

Notification::~Notification() = default;

}; // namespace brls
```

## 改动总结

| 文件 | 新增 | 修改 |
|---|---|---|
| notification_manager.hpp | NotificationStyle 结构体、setStyle 方法、customStyle 静态成员、friend 声明 | 无删除 |
| notification_manager.cpp | setStyle 实现 | 构造函数读 customStyle、notify 读 customStyle |

## 向后兼容性

- 不调用 `setStyle` → customStyle 全零 → 构造时从 metric 填入 → 行为不变
- `Application::notify("text")` 接口不变
- 不硬编码框架数值，框架作者改 metric 仍然生效
- 不新增头文件依赖（NVGcolor 已在 include 链中）
- C++17 兼容