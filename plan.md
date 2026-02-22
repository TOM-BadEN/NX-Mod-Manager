# LB/RB 翻页方案（待定）

## 现状

Home 和 ModList 各自实现了 `flipScreen(int direction)` 方法，逻辑 99% 相同。
ActionMenu 也需要翻页（按键映射已列出 LB/RB），但尚未实现。

## 现有 flipScreen 逻辑

1. 找当前焦点 Cell 的 index（向上遍历 parent 链找 RecyclingGridItem）
2. 计算目标 index：`idx + direction * spanCount * rowsPerScreen`
3. `std::clamp` 边界钳位到 `[0, itemCount-1]`
4. 已在首/末尾时 `shakeHighlight` 抖动反馈
5. `selectRowAt(target)` 确保 Cell 可见 → `getGridItemByIndex(target)` 取 Cell → `giveFocus`
6. 临时设 `brls/animations/highlight` 为 1ms 防高亮闪烁

## Home 与 ModList 的唯一差异

`rowsPerScreen` 计算公式不同：

- **Home**：`getHeight() / estimatedRowHeight`（漏了 estimatedRowSpace）
- **ModList**：`getHeight() / (estimatedRowHeight + estimatedRowSpace)`（正确公式）

Home 版本会略微高估每屏行数，因为实际每行占 rowHeight + rowSpace。
影响不大（Home Cell 很大），但公式上 ModList 是正确的。

## flipScreen 用到的数据全部来自 RecyclingGrid

| 数据 | 来源 |
|---|---|
| `getCurrentFocus()` | 全局 API |
| `RecyclingGridItem::getIndex()` | Cell 方法 |
| `getHeight()`, `estimatedRowHeight`, `estimatedRowSpace`, `spanCount` | RecyclingGrid 成员 |
| `getDataSource()->getItemCount()` | RecyclingGrid 方法 |
| `selectRowAt()`, `getGridItemByIndex()` | RecyclingGrid 方法 |
| `giveFocus()` | 全局 API |
| Style metric（高亮 trick） | 全局 API |

零外部依赖，适合提取为 RecyclingGrid 成员方法。

## 方案对比

### 方案 A：完全内置（方法 + 注册）

RecyclingGrid 新增 `flipScreen(int direction)` 方法，并在构造函数中注册 LB/RB action。

- **优点**：外部零代码，所有 RecyclingGrid 自动获得翻页能力
- **缺点**：LB/RB 的 hidden 属性无法按使用方定制（Home/ModList 需要 hidden=true，ActionMenu 可能需要 hidden=false）

### 方案 B：方法内置，注册外部

RecyclingGrid 只新增 `flipScreen(int direction)` 公开方法，不内部注册按键。
各使用方自行注册 LB/RB 并调用 `m_grid->flipScreen(direction)`。

- **优点**：消除重复的 flipScreen 逻辑，保留注册灵活性（hidden/visible 由使用方控制）
- **缺点**：每个使用方仍需 2 行注册代码

### 对现有代码的影响

| 方案 | Home/ModList 改动 | ActionMenu 改动 | RecyclingGrid 改动 |
|---|---|---|---|
| A（完全内置） | 删除 flipScreen 方法和 LB/RB 注册 | 无需任何翻页代码 | 加 flipScreen + 构造函数注册 LB/RB |
| B（方法内置） | 删除 flipScreen 方法，LB/RB 注册改为调 `m_grid->flipScreen()` | 注册 LB/RB 调 `m_grid->flipScreen()` | 只加 flipScreen 方法 |

## 决策

**方案 A（完全内置）**，已实施。flipScreen 方法 + LB/RB 注册内置于 RecyclingGrid，Home/ModList/ActionMenu 自动获得翻页能力。