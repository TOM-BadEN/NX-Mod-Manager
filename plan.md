# 详情面板滚动 + 焦点切换 — 实施计划

## 目标

- MOD 描述正文过长时，在卡片内部滚动（图标/标签/标题固定可见）
- 列表按右切焦点到详情，详情按左切回列表（恢复原位置）
- 选中框画在整个卡片上，样式与原生高亮完全一致（发光+渐变+圆角）

## 方案

### 核心思路

焦点放在 ScrollingFrame 内部的 focusable 元素上（原生滚动生效），
但用 `hideHighlight` 隐藏真实高亮，
在外层卡片上通过自定义 `HighlightBox` 手动控制 `highlightAlpha` 画假高亮。

### HighlightBox

继承 `brls::Box` 的轻量子类，唯一功能：暴露 `protected` 的 `highlightAlpha` 控制。
复用原生 `drawHighlight()` 绘制，效果与真实高亮完全一致。

```cpp
class HighlightBox : public brls::Box {
public:
    void setFakeHighlight(bool on) { this->highlightAlpha = on ? 1.0f : 0.0f; }

    // 重写 frame：Box::frame() 画高亮背景，frameHighlight() 画高亮边框
    // 系统只对 currentFocus 调 frameHighlight，HighlightBox 不是焦点所以需要手动调
    void frame(brls::FrameContext* ctx) override {
        Box::frame(ctx);
        this->frameHighlight(ctx);
    }

    static brls::View* create() { return new HighlightBox(); }
};
```

文件位置：`code/include/view/highlightBox.hpp`（纯头文件，无 .cpp）

### 改造后结构

```
content (row)
  ├── grid (50%)
  └── HighlightBox (id=modList/card, 卡片样式, grow=1, margin*, padding*, highlightCornerRadius)
      ├── 图标 + 游戏名/TID
      ├── 标签区 (tagRow, flexWrap)
      ├── "MOD描述" 标题
      └── ScrollingFrame (id=modList/scroll, width=100%, grow=1)
          └── descBox (Box, id=modList/descBox, focusable, hideHighlight)
              └── descBody (Label)
```

- **HighlightBox**：卡片外壳，正常布局 grow 撑满，不被 detach，假高亮完整覆盖
- **ScrollingFrame**：只包裹描述正文区域，在卡片内部局部滚动
- **descBox**：focusable + hideHighlight，真正接收焦点，ScrollingFrame 检测 childFocused 启用原生滚动

---

## 改动清单

### 0. 新建 HighlightBox（view/highlightBox.hpp）

- [ ] 创建 `code/include/view/highlightBox.hpp`
- [ ] 继承 `brls::Box`，暴露 `setFakeHighlight(bool)` + `create()`
- [ ] 在 `main.cpp` 或 `modList.cpp` 中 `registerXMLView("HighlightBox", HighlightBox::create)`

### 1. XML（modList.xml）

- [ ] 外层 detail Box 改为 `<HighlightBox id="modList/card">`，保留全部卡片样式
- [ ] 图标、标签、描述标题直接放在 HighlightBox 内（不滚动）
- [ ] descBody 包在 `<brls:ScrollingFrame id="modList/scroll">` + `<brls:Box id="modList/descBox" focusable="true" hideHighlight="true">` 内
- [ ] HighlightBox 新增 `highlightCornerRadius="8"`

### 2. HPP（modList.hpp）

- [ ] 新增 `BRLS_BIND(HighlightBox, m_card, "modList/card")`
- [ ] 新增 `BRLS_BIND(brls::ScrollingFrame, m_scroll, "modList/scroll")`
- [ ] 新增 `BRLS_BIND(brls::Box, m_descBox, "modList/descBox")`
- [ ] 新增 `size_t m_lastFocusIndex = 0;`
- [ ] `#include "view/highlightBox.hpp"`

### 3. CPP（modList.cpp）

- [ ] `setupDetail()` 中：
  - `m_scroll->setScrollingIndicatorVisible(false)` — 隐藏滚动条
  - 订阅 `m_descBox->getFocusEvent()` → `m_card->setFakeHighlight(true)`
  - 订阅 `m_descBox->getFocusLostEvent()` → `m_card->setFakeHighlight(false)`
  - `m_descBox` 注册 `NAV_LEFT` → 重置滚动 + `giveFocus` 回列表
- [ ] `setupModGrid()` 中：
  - `m_grid` 注册 `NAV_RIGHT` → `giveFocus(m_descBox)` 切到详情
- [ ] `updateDetail()` 中：
  - `m_lastFocusIndex = index` — 记录当前焦点位置
  - `m_scroll->setContentOffsetY(0, false)` — 切换 MOD 时重置滚动

---

## 焦点流程

1. 用户在列表第 N 个卡片 → 按 **右** → `giveFocus(m_descBox)`
2. `m_descBox` 获焦 → `focusEvent` 触发 → `m_card->setFakeHighlight(true)` → 卡片显示高亮
3. ScrollingFrame 检测 `childFocused=true` → 上下键自动滚动描述正文
4. 用户按 **左** → 重置滚动 + `giveFocus(grid[N])` → `focusLostEvent` → `m_card->setFakeHighlight(false)`

## 注意事项

- `scrollingIndicatorVisible` 非 XML 注册属性，必须 C++ 调用
- `hideHighlight` 是 XML 注册属性 ✅
- `focusable` 是 XML 注册属性 ✅
- `highlightAlpha` 是 `View` 的 protected 成员，HighlightBox 子类可访问
- `drawHighlight()` 背景部分在 `View::frame()` 中自动调用（`highlightAlpha > 0`）
- `frameHighlight()` 边框部分系统只对 `currentFocus` 调用 → HighlightBox 在重写的 `frame()` 中手动调用（已在代码示例中解决）
