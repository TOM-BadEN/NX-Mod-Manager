# ActionMenu — 通用右侧菜单组件设计方案

## 一、概述

自建一个通用右侧弹出菜单组件 `ActionMenu`，覆盖以下所有场景：

| 能力 | 场景举例 |
|---|---|
| **单选** | 主页菜单：收藏 / 改名 / 删除 / 设置 → 选一个执行 |
| **多级菜单** | 主菜单 → 选"设置" → 菜单内容切换为设置子菜单 |
| **多选** | 添加 mod → 列出 100 个文件 → 勾选多个 → 确认安装 |

## 二、数据模型

```cpp
struct MenuPageConfig;  // 前向声明

enum class AfterAction {
    CloseMenu,  // 退出整个菜单（默认）
    PopPage,    // 返回上一级（子菜单选择场景，栈空则自动关闭）
    Stay,       // 留在当前页（开关类选项）
};

struct MenuItemConfig {
    // 基础字段
    std::string titleText;
    std::string hintText;
    std::string badgeText;
    std::function<void()> onAction              = nullptr;
    MenuPageConfig* subMenuPtr                  = nullptr;
    bool enabled                                = true;
    bool selected                               = false;  // 多选模式（ActionMenu 内部管理）
    AfterAction afterAction                     = AfterAction::CloseMenu;

    // 动态覆盖（可选，运行时优先于静态值）
    std::function<std::string()> titleGetter    = nullptr;
    std::function<std::string()> badgeGetter    = nullptr;

    // 链式方法
    MenuItemConfig& title(const std::string& s);
    MenuItemConfig& title(std::function<std::string()> f);  // 动态覆盖
    MenuItemConfig& badge(const std::string& s);
    MenuItemConfig& badge(std::function<std::string()> f);  // 动态覆盖
    MenuItemConfig& action(std::function<void()> a);
    MenuItemConfig& submenu(MenuPageConfig* s);
    MenuItemConfig& disabled();
    MenuItemConfig& popPage();   // 执行 action 后返回上一级（子菜单选择场景）
    MenuItemConfig& stayOpen();  // 执行 action 后留在当前页（开关类选项）

    // ActionMenu 内部取值
    std::string getTitle() const;  // titleGetter ? titleGetter() : titleText
    std::string getBadge() const;  // badgeGetter ? badgeGetter() : badgeText
};

struct MenuPageConfig {
    std::string title;                  // 页面标题
    std::vector<MenuItemConfig> items;  // 选项列表
    bool multiSelect = false;
    std::function<void(const std::vector<int>&)> onConfirm = nullptr;  // 多选确认
    std::function<void()> onDismiss = nullptr;                          // 取消

    void show();  // 弹出菜单
};
```

## 三、多级菜单机制

用 **栈（stack）** 管理菜单层级。栈元素不仅存页面指针，还存焦点索引以便返回时恢复位置：

```cpp
struct MenuStackEntry {
    MenuPageConfig* page;
    size_t focusIndex;  // pushPage 时保存，popPage 时恢复
};
std::vector<MenuStackEntry> m_menuStack;
```

```
用户打开菜单 → push({主菜单页, 0})
菜单栈: [{主菜单页, 0}]               ← 显示主菜单

用户在第3项选 "设置" → push({设置页, 0})，保存父焦点 index=2
菜单栈: [{主菜单页, 2}, {设置页, 0}]  ← 显示设置页

用户按 B → pop() → 恢复焦点到 index=2
菜单栈: [{主菜单页, 2}]               ← 回到主菜单，焦点在"设置"项

用户按 B → pop() → 栈空 → 关闭菜单 + 触发 onDismiss
```

选中逻辑：
- `subMenuPtr != nullptr` → pushPage(子菜单)
- `onAction != nullptr` → 执行 action，然后根据 `afterAction` 分三种处理：
  - `CloseMenu`（默认）→ 退出整个菜单
  - `PopPage` → 返回上一级（栈空则自动关闭），父菜单 Cell 通过 getBadge() 自动更新
  - `Stay` → 留在当前页，直接更新当前 Cell 的 title/badge
- 多选模式下 A 键 → ActionMenu 内部翻转 `selected`

## 四、回调说明

每个菜单项的回调直接在 `MenuItemConfig::onAction` 中注册，不需要集中式回调。

| 回调 | 位置 | 说明 |
|---|---|---|
| `onAction` | `MenuItemConfig` | 单选模式下选中执行，默认关闭菜单 / `.popPage()` 返回上级 / `.stayOpen()` 保持打开 |
| `subMenuPtr` | `MenuItemConfig` | 指向子菜单，与 onAction 互斥 |
| `onConfirm` | `MenuPageConfig` | 多选模式下用户按 + 键时触发 |
| `onDismiss` | `MenuPageConfig` | 用户按 B 关闭整个菜单时触发 |

### 多选勾选状态管理

多选模式下，**ActionMenu 内部自动管理** `MenuItemConfig::selected` 的切换：
- 用户按 A → ActionMenu 自动翻转当前项的 `selected`，更新 UI
- 用户按 + → ActionMenu 收集所有 `selected=true` 的项的 index，触发 `onConfirm`

### 生命周期

`MenuPageConfig::show()` 内部流程：
1. 如果 `multiSelect == true`，自动重置所有 `items[i].selected = false`（防止跨次残留）
2. `new ActionMenu(this)` 创建实例，传入 MenuPageConfig 指针
3. `new brls::Activity(actionMenu)` 包装为 Activity
4. `brls::Application::pushActivity(activity)` 推入栈
5. Activity 被 pop 时自动 `delete` ActionMenu（Borealis 框架管理）
6. MenuPageConfig 本身是成员变量，不被 delete，可重复使用

## 五、调用方式

### 5.1 单选 + 多级菜单

```cpp
// ---- homeTab.hpp 成员变量 ----
MenuPageConfig m_menuSettings;
MenuPageConfig m_menuMain;

// ---- homeTab.cpp 构造函数 ----

// 注册子菜单
m_menuSettings = {"设置", {
    MenuItemConfig{"语言", "切换界面语言"}
        .badge([this]{ return getCurrentLang(); })
        .action([this]{ changeLang(); })
        .stayOpen(),
    MenuItemConfig{"永久删除", "删除原始 mod 文件"}
        .badge([this]{ return isPermDelete() ? "开启" : "关闭"; })
        .action([this]{ togglePermDelete(); })
        .stayOpen(),
}};

// 注册主菜单
m_menuMain = {"操作", {
    MenuItemConfig{"收藏游戏", "将此游戏添加/移出收藏列表"}
        .title([this]{ return isFavorite() ? "取消收藏" : "收藏游戏"; })
        .action([this]{ toggleFavorite(); }),
    MenuItemConfig{"自定义名称", "修改游戏的显示名称"}
        .action([this]{ renameGame(); }),
    MenuItemConfig{"设置", "应用设置"}
        .submenu(&m_menuSettings),
    MenuItemConfig{"移除游戏", "从列表中移除此游戏"}
        .action([this]{ removeGame(); }),
}};

// X 键打开菜单
registerAction("菜单", brls::BUTTON_X, [this](...) {
    m_menuMain.show();
    return true;
});
```

### 5.2 多选

```cpp
// ---- homeTab.hpp 成员变量 ----
MenuPageConfig m_menuModSelect;

// ---- homeTab.cpp 使用 ----
m_menuModSelect.title = "选择 Mod";
m_menuModSelect.multiSelect = true;
m_menuModSelect.items = buildModItems();  // 动态构建，可能 100+
m_menuModSelect.onConfirm = [this](const std::vector<int>& indices) {
    installMods(indices);
};
m_menuModSelect.show();
// ↑ m_menuModSelect 是成员变量，生命周期跟 HomeTab 一样长
// ↑ ActionMenu 内部持有指针，不会悬空
```

## 六、UI 布局

```
┌──────────────────────┬─────────────────┐
│                      │                 │
│                      │   操作          │
│                      │ ─────────────── │ ← 分界线 (lineBottom)
│  ┌───────────────┐   │                 │
│  │               │   │  ▸ 收藏游戏     │
│  │  将此游戏添加  │   │                 │
│  │  到收藏列表    │   │  ─────────────  │
│  │               │   │    改名         │
│  │  收藏后游戏会  │   │                 │
│  │  显示在列表    │   │  ─────────────  │
│  │  最前面       │   │    设置     ▸   │
│  │               │   │                 │
│  └───────────────┘   │  ─────────────  │
│                      │    ...          │
│  ↑ 提示词卡片         │                 │
│  随焦点动态更新       │ ─────────────── │ ← 分界线 (lineTop)
│                      │ 1/4    Ⓑ返回 Ⓐ确认│
└──────────────────────┴─────────────────┘
                         ↑索引   ↑brls::Hints
```

> **brls::Hints 工作原理**：订阅 `globalHintsUpdateEvent`，焦点变化时从当前焦点 View
> 沿 parent 链向上收集所有 `registerAction` 注册的 Gamepad Action，自动渲染按钮图标+文字。
> ActionMenu 只需 `registerAction` 注册 A/B/X/+ 等按键，Hints 自动显示，无需手动代码。

### 透明叠加：isTranslucent() 覆写

ActionMenu 必须覆写 `isTranslucent()` 返回 `true`（与 Dropdown 一致），否则淡入动画结束后
Borealis 渲染循环不会绘制底层 Activity，导致半透明遮罩后面变黑。

```cpp
bool isTranslucent() override {
    return true || View::isTranslucent();
}
```

### 提示词卡片联动机制

使用 `RecyclingGrid::setFocusChangeCallback`（现成 API），焦点变化时：
1. 回调参数为新焦点的 `size_t index`
2. 从当前 MenuPageConfig 的 `items[index].hintText` 获取提示词
3. 更新左侧提示词卡片 Label 文字
4. hintText 为空时隐藏卡片，非空时显示

### 关键组件

| 组件 | 职责 |
|---|---|
| `ActionMenu` | 顶层容器，管理菜单栈、遮罩、动画、按键注册 |
| `ActionMenuCell` | RecyclingGridItem 子类，单个选项 UI |
| `ActionMenuDataSource` | RecyclingGridDataSource 实现，从当前 MenuPageConfig 提供数据 |

### 选项列表用 RecyclingGrid 的理由

- 项目内风格统一（Home 页 GameCard 网格也用它）
- `setFocusChangeCallback` 现成，提示词卡片联动无需额外重写
- Cell 基类 `RecyclingGridItem` 与 `GameCard` / `ModCard` 一致
- 少量选项（6-8）：正常工作，无额外开销
- 大量选项（100+）：虚拟化回收，内存中只有可见 ~8 个 Cell
- `spanCount=1` 即为单列列表

## 七、技术架构

```
ActionMenu (继承 brls::Box, translucent overlay)
├── 左侧区域 (Box, grow=1)
│   ├── 半透明遮罩背景
│   └── 提示词卡片 (Box, 圆角背景, 居中偏右)
│       └── hint Label (多行自动换行, app/textSecondary 色)
└── 右侧面板 (Box, 固定宽度 ~屏幕1/3, 仿 myframe.xml 精简结构)
    ├── Header (Box, lineBottom=1px 分界线)
    │   └── 标题 Label (id: actionMenu/title)
    ├── Content (Box, grow=1)
    │   └── RecyclingGrid (spanCount=1, estimatedRowHeight=70, estimatedRowSpace=0)
    │       └── ActionMenuCell (RecyclingGridItem)
    │           ├── title Label（靠左）
    │           ├── badge Label（app/textHighlight 色，靠右，可选）
    │           └── 多选勾选图标 (Image, 仅多选模式显示)
    └── Footer (Box, lineTop=1px 分界线, justifyContent=spaceBetween)
        ├── 索引 Label (id: actionMenu/index, 左侧, 如 "1/4")
        └── brls::Hints (右侧, 自动读取 registerAction 显示按钮图标+文字)
```

## 八、文件清单

### 需要新建的文件

| 文件 | 用途 |
|---|---|
| `code/include/view/actionMenu.hpp` | MenuItemConfig + MenuPageConfig + ActionMenu + ActionMenuCell 声明 |
| `code/src/view/actionMenu.cpp` | ActionMenu 实现（栈管理、动画、DataSource）+ 链式方法实现 |
| `resources/xml/view/actionMenu.xml` | 精简版框架布局（仿 myframe.xml，去掉时间/WiFi/电池）：Header(标题+分界线) + Content(空Box) + Footer(索引Label + brls::Hints + 分界线) + 左侧 hint 卡片 |
| `resources/xml/view/actionMenuCell.xml` | 单个选项项布局（title + badge + 勾选图标） |

### 需要改动的已有文件

| 文件 | 改动 |
|---|---|
| `code/src/main.cpp` | 添加 `registerXMLView("ActionMenuCell", ActionMenuCell::create)` |
| `code/src/view/recyclingGrid.cpp` | `onChildFocusGained` 加 `m_defaultCellFocus = idx` + `queueReusableCell` 加 `cell->setParent(nullptr)`（见 11.8） |
| `code/src/activity/home.cpp` | `toggleSort` 加 `setDefaultCellFocus(0)` + 集成 ActionMenu |
| `code/src/activity/modList.cpp` | `toggleSort` 加 `setDefaultCellFocus(0)` |

> **CMakeLists.txt 不需要改动**：`file(GLOB_RECURSE MAIN_SRC code/*.cpp)` 会自动扫描新增的 `.cpp` 文件。

## 九、按键映射

| 按键 | 单选模式 | 多选模式 |
|---|---|---|
| **上/下** | 移动光标 | 移动光标 |
| **A** | 选中执行 | 切换勾选 |
| **B** | 返回上级 / 关闭 | 返回上级 / 关闭 |
| **X** | — | 全选 / 全不选 |
| **+** | — | 确认提交（触发 confirmCallback） |
| **LB/RB** | 翻页 | 翻页 |

## 十、实现步骤

1. 创建 actionMenu.hpp：MenuItemConfig / MenuPageConfig + 链式方法声明 + ActionMenuCell + ActionMenu 类声明
2. 创建 XML 布局：actionMenu.xml（右侧面板 + 左侧提示词卡片）+ actionMenuCell.xml（title + badge + lineBottom 分隔线）
3. 创建 actionMenu.cpp：链式方法实现 + ActionMenuCell + ActionMenu 构造/show/hide/动画
4. 实现 RecyclingGridDataSource：从当前 MenuPageConfig 提供行数/Cell，用 getTitle()/getBadge() 取值
5. 实现菜单栈：pushPage / popPage / 标题更新 / reloadData
6. 实现单选逻辑：onItemSelected → 有 subMenuPtr 则 pushPage，有 onAction 则执行 → AfterAction::CloseMenu/PopPage/Stay 三分支处理
7. 实现多选模式：A 键 → 内部翻转 selected → 更新 Cell UI；+ 键 → 收集 indices → onConfirm → popActivity
8. 实现 B 键处理：覆盖默认行为，pop 栈，栈空则 popActivity + 触发 onDismiss
9. 实现提示词卡片联动 + 索引更新：setFocusChangeCallback → 更新左侧卡片 hint 文字 + 更新底部索引 Label（如 "2/4"）
10. 集成到 Home 页：X 键注册，用 MenuItemConfig{} 构建菜单页，.show() 弹出

## 十一、实现注意事项

### 11.1 Stay 模式下更新单个 Cell，不要 reloadData

`AfterAction::Stay` 执行后，**禁止调用 `reloadData()`**（会重建所有 Cell 导致焦点丢失）。
正确做法：通过 `RecyclingGrid::getGridItemByIndex(index)` 拿到当前可见 Cell，直接更新其 title/badge Label。

### 11.2 防止重复 show()

`MenuPageConfig::show()` 内部需加守卫，避免连续按键推入多个 Activity。
方案：ActionMenu 持有一个 `static bool s_isOpen` 标志，`show()` 时检查，`popActivity` 回调中重置。

### 11.3 初始焦点自动触发 hint 更新

`RecyclingGrid::onChildFocusGained` 在首次获得焦点时即会触发回调（已验证源码），
因此菜单打开后提示词卡片会自动填充第一项的 hintText，无需手动初始化。

### 11.4 onDismiss 在菜单完全关闭时触发（任何关闭方式）

无论 B 键关闭还是 CloseMenu 动作关闭，根菜单的 `onDismiss` 均会触发.
子菜单通过栈 pop 返回父菜单，不触发子菜单的 `onDismiss`。
用途：做资源清理或通知调用方刷新 UI，**不要用来执行业务逻辑**（业务逻辑应在 `onAction` 中完成）。

### 11.5 MenuPageConfig 必须是成员变量

所有 `MenuPageConfig` 实例必须作为调用方的 **成员变量**（如 `HomeTab::m_menuMain`），
确保生命周期覆盖整个 ActionMenu 显示期间。局部变量会导致悬空指针。

### 11.6 PopPage 返回父菜单时自动刷新 badge 并恢复焦点

`AfterAction::PopPage` 执行后，ActionMenu 弹出栈顶并对父菜单执行 `reloadData()`，
使父菜单 Cell 通过 `getBadge()` 获取最新值（如子菜单中改了语言，父菜单的 badge 要反映新值）。

焦点恢复：popPage 前从 `MenuStackEntry::focusIndex` 取出父菜单的焦点位置，
调用 `RecyclingGrid::setDefaultCellFocus(savedIndex)` 后再 `reloadData()`，
这样 reloadData 内部会自动定位到该索引（已验证 reloadData 源码第 174 行使用 m_defaultCellFocus）。

### 11.7 多选模式 show() 自动重置 selected 状态

`MenuPageConfig::show()` 内部对多选模式自动执行 `for (auto& item : items) item.selected = false`.
防止上次未确认的勾选残留到下次打开。调用方无需手动清理。

### 11.8 底层焦点恢复（RecyclingGrid 改 2 行 + 现有页面各改 1 行）

**问题**：Borealis 的 `focusStack` 存 View 原始指针。ActionMenu 打开期间若底层 RecyclingGrid
执行 `reloadData()`（如删除游戏），被保存的 Cell 指针变成已回收的不可见对象，popActivity
恢复焦点时指向错误位置。

**方案**：

1. **`RecyclingGrid::onChildFocusGained`** 加 `m_defaultCellFocus = idx`
   — 每次聚焦自动记住索引，reloadData 使用该索引作为起始位置（内置越界钳位）
2. **`RecyclingGrid::queueReusableCell`** 加 `cell->setParent(nullptr)`
   — 回收 Cell 断开 parent 链，popActivity 检测到 `getParentActivity()==nullptr` 自动走
   fallback：`giveFocus(底层 Activity contentView)` → 沿 `lastFocusedView` 链向下
   → 到达 reloadData 设置的正确 Cell
3. **Home::toggleSort / ModList::toggleSort** 在 `reloadData()` 前加 `setDefaultCellFocus(0)`
   — 排序后明确回到顶部，防止 auto-track 导致停留在旧位置

**场景验证**：

| 场景 | 焦点位置 |
|---|---|
| 删中间的游戏 | 被删位置的下一个（原 index+1 顶上来） |
| 删最后位置的游戏 | 上一个（钳位到 itemCount-1） |
| 删唯一的游戏 | 其他 UI 元素（Tab 栏等） |
| 排序 | 第一项（setDefaultCellFocus(0)） |
| 打开菜单什么都没做就关 | 原来位置（Cell 未被回收，Borealis 正常恢复） |

**调用方零负担**：onAction 可直接 `deleteGame() + reloadData()`，不需要标记位、onDismiss
或手动 giveFocus。焦点自动恢复到正确位置。

### 11.9 onDismiss 在菜单任何方式关闭时均触发

不限于 B 键关闭。CloseMenu 动作关闭、B 键关闭均触发 `onDismiss`。
调用方可在 onDismiss 中做统一的 UI 刷新（如有需要），但焦点恢复已由底层自动处理（见 11.8）。

### 11.10 删除操作需检查是否删完

onAction 中执行删除（游戏/Mod）后，必须检查列表是否为空。如果删完了，需要对应处理：

```cpp
.action([this]{
    removeGame(index);
    if (m_games.empty()) {
        // 列表已空：隐藏 Grid，显示空提示，关闭菜单后焦点自动到提示 Label
        showEmptyHint();
    } else {
        homeGrid->reloadData();  // 还有数据：正常 reload，焦点自动恢复（见 11.8）
    }
})
```

原因：`RecyclingGrid::getDefaultFocus()` 在 `itemCount==0` 时返回 `nullptr`，
Borealis fallback 会把焦点给页面上其他可聚焦 View。如果此时 Grid 仍可见但为空，
用户可能看到空白区域且无法操作。应主动切换到空提示状态。