# Mod 页面设计

## 页面结构

复用 MyFrame 框架，左右分栏布局。

```
┌─────────────────────────────────────────────────┐
│  MyFrame Header: 游戏名 + 图标                   │
├──────────────────┬──────────────────────────────┤
│                  │                              │
│  ModList         │   ModDetail                  │
│  (卡片列表)      │   (文本排版详情，暂占位)       │
│                  │                              │
│  ┌────────────┐ │                              │
│  │🎮 mod-A    │ │                              │
│  │  文件夹 ✅ │ │                              │
│  └────────────┘ │                              │
│  ┌────────────┐ │                              │
│  │🎮 mod-B    │ │                              │
│  │  ZIP   ❌  │ │                              │
│  └────────────┘ │                              │
│  ┌────────────┐ │                              │
│  │🎮 mod-C    │ │                              │
│  │  文件夹 ✅ │ │                              │
│  └────────────┘ │                              │
│  ┌────────────┐ │                              │
│  │🎮 xx.zip   │ │                              │
│  │  ZIP   ❌  │ │                              │
│  └────────────┘ │                              │
│                  │                              │
├──────────────────┴──────────────────────────────┤
│  3/12                             B返回  A确认   │
└─────────────────────────────────────────────────┘
```

- Footer 左下角索引：`当前选中 / 总 mod 数`
- Footer 右侧：按键提示（由 MyFrame 的 brls::Hints 自动管理）

## ModItem 组件

### 卡片外壳

复用 GameCard 的视觉样式：
- `backgroundColor="@theme/app/cardBg"`
- `cornerRadius="8"`, `highlightCornerRadius="8"`
- `shadowType="generic"`
- `focusable="true"`
- padding 四周 20px

### 内部布局（水平排列）

```
┌───────┬──────────────────────┬──────────┐
│ Icon  │  mod名称（22号）      │ 安装状态  │
│ 48x48 │  类型（17号，灰色）   │          │
└───────┴──────────────────────┴──────────┘
  左对齐     中间 grow=1          右对齐
```

- **左**：mod 图标（brls::Image，48x48）
- **中**：名称 + 类型（brls::Box 纵向，名称 fontSize=22，类型 fontSize=17 灰色）
- **右**：安装状态文本

### 公开接口

```cpp
void setMod(const std::string& name, const std::string& type, bool installed);
void clear();  // 隐藏卡片
static brls::View* create();  // XML 工厂
```

### 文件

- `include/view/modItem.hpp`
- `src/view/modItem.cpp`
- `resources/xml/view/modItem.xml`

## ModList 组件

### 设计模式

复用 GridPage 的实现模式：
- XML 预定义 4 个 ModItem 槽位（纵向排列，margin 间距）
- 构造函数通过 getView 获取槽位引用
- 外部数据指针 `std::vector<ModInfo>*`
- 逐行滚动通过 scrollOffset + refreshItems

### 核心成员

```cpp
class ModList : public brls::Box {
public:
    ModList();
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    void setModList(std::vector<ModInfo>& mods);
    void reloadData();
    void setIndexChangeCallback(std::function<void(int, int)> callback);

    static brls::View* create();

private:
    static constexpr int ITEMS_PER_PAGE = 4;

    ModItem* m_items[ITEMS_PER_PAGE];
    std::vector<ModInfo>* m_mods = nullptr;
    int m_scrollOffset = 0;
    IndexUpdate m_indexUpdate;

    void refreshItems();
    int findFocusedItemIndex();
    bool isItemVisible(int index);
};
```

### 逐行滚动逻辑（getNextFocus）

```
items[i] 显示 mods[m_scrollOffset + i]

DOWN:
  i < 3 且 items[i+1] 可见 → 返回 items[i+1]
  i == 3 且 scrollOffset + 4 < total → scrollOffset++, refreshItems(), 返回 items[3]
  否则 → nullptr（到底了）

UP:
  i > 0 → 返回 items[i-1]
  i == 0 且 scrollOffset > 0 → scrollOffset--, refreshItems(), 返回 items[0]
  否则 → nullptr（到顶了）
```

滚动时焦点不移动（停留在边界槽位），数据刷新。用户感觉列表在滚动。

### 焦点变化回调

焦点变化时通知外部（ModActivity）当前选中的全局索引和总数。
用于更新右侧 ModDetail + 底部索引文本。
复用 IndexUpdate 工具类。

### 文件

- `include/view/modList.hpp`
- `src/view/modList.cpp`
- `resources/xml/view/modList.xml`

## ModDetail 组件（暂占位）

- 纯展示，不可聚焦
- 各种 Label 文本排版
- 由 ModList 焦点变化驱动内容更新
- 后续补充具体字段

### 文件

- `include/view/modDetail.hpp`
- `src/view/modDetail.cpp`
- `resources/xml/view/modDetail.xml`

## 页面 Activity

### ModActivity

- 替代现有 SecondActivity
- onContentAvailable 中：扫描 mod 目录 → 设置 ModList → 设置回调
- 接收 GameInfo 数据（从主页传入）

### 文件

- `include/activity/mod_activity.hpp`
- `src/activity/mod_activity.cpp`
- `resources/xml/activity/mod.xml`（左右分栏布局）

## 数据结构

```cpp
struct ModInfo {
    std::string name;       // mod 名（目录名或文件名）
    std::string type;       // 类型描述
    bool isInstalled;       // 是否已安装
    std::string path;       // 完整路径
};
```

### 文件

- `include/common/modInfo.hpp`

## 新增文件清单

| 文件 | 说明 |
|---|---|
| include/view/modItem.hpp | 单个卡片组件 |
| src/view/modItem.cpp | 卡片组件实现 |
| resources/xml/view/modItem.xml | 卡片布局 |
| include/view/modList.hpp | 列表组件 |
| src/view/modList.cpp | 列表组件实现 |
| resources/xml/view/modList.xml | 列表布局（4 个 ModItem） |
| include/view/modDetail.hpp | 详情组件（占位） |
| src/view/modDetail.cpp | 详情组件实现（占位） |
| resources/xml/view/modDetail.xml | 详情布局（占位） |
| include/activity/mod_activity.hpp | Mod 页面 Activity |
| src/activity/mod_activity.cpp | Mod 页面实现 |
| resources/xml/activity/mod.xml | 页面布局（左右分栏） |
| include/common/modInfo.hpp | ModInfo 数据结构 |

## 待确认（后续）

1. mod 启用/禁用判断机制
2. 用户可执行的操作（启用/禁用/删除等）
3. GameCard 点击跳转到 Mod 页面的实现
4. ModDetail 具体显示哪些字段
5. Mod 类型图标资源
6. L/R 翻页是否需要
