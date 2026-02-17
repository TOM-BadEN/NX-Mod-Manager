# GridPage → RecyclingGrid 重构计划

## 一、背景

当前 `GridPage` 为所有数据项创建 View（O(N)），存在以下问题：

| 问题 | 严重度 | 说明 |
|------|--------|------|
| 悬空指针 | 🔴 高 | `setItemCount` → `rebuild` → `clearViews` 销毁焦点 View，`giveFocus` 访问已释放内存（UB） |
| 布局时序 | 🔴 高 | `setData` 中 `giveFocus` 在 Yoga 布局完成前执行，导致滚动数据错误 |
| 内存 O(N) | 🟡 中 | 商店页面 5000+ 游戏时，全量创建 View 不可接受 |
| 引用计数泄漏 | 🟡 中 | 每次 `rebuild` 默认图标 count 只增不减 |
| 全量绑定 | 🟡 中 | `bindAll` 一次性绑定所有数据，数据量大时卡顿 |
| 不支持分页 | 🟡 中 | 商店需要无限滚动 + 按需加载 |

## 二、目标

用参考项目（wiliwili）的 `RecyclingGrid` 替换 `GridPage`：
- **只为可视区域创建 ~15 个 View**（O(1) 内存）
- **Cell 回收复用**（无悬空指针）
- **`layouted` 延迟**（布局完成后才操作 View）
- **`onNextPage` 分页**（商店无限滚动）
- **UI 样式完全不变**（GameCard + gameCard.xml 不改样式）
- **通用组件**：一套 RecyclingGrid 适用于主页网格、商店网格、Mod 列表等所有场景

## 三、参考来源

```
reference\willwill\wiliwili\source\view\recycling_grid.cpp     — 多列网格回收实现 (~760行)
reference\willwill\wiliwili\include\view\recycling_grid.hpp     — 头文件
```

borealis 已有的基础：
```
library\borealis\library\include\borealis\views\recycler.hpp   — RecyclerCell / RecyclerFrame 基类
library\borealis\library\lib\views\recycler.cpp                — 单列回收实现
```

RecyclingGrid 不使用 borealis 的 RecyclerFrame，而是直接继承 `ScrollingFrame` 自行实现多列网格回收。

---

## 四、场景分析

### 场景 1：主页（~200 个游戏，两阶段异步加载）

**数据特点：**
- 第一阶段：本地扫描，`m_games` 一次性填充 ~200 条，displayName / version / modCount 立即可用
- 第二阶段：后台线程逐个调 `nsGetApplicationControlData` 获取图标 JPEG + 最新名称/版本
- 图标通过 `nvgCreateImageMem` 创建 NVG 纹理，存入 `m_games[idx].iconId`

**RecyclingGrid 下的数据流：**

```
时刻 0：onContentAvailable()
  ├── m_games = GameScanner().scanGames()        ← 200 个 GameInfo，文字数据已有
  ├── 所有 game.iconId == 0                      ← 图标还没加载
  ├── setupGridPage()                            ← 设置 DataSource
  └── startNacpLoader()                          ← 启动后台线程

时刻 1：首帧 onLayout()（~16ms 后）
  ├── layouted = true → reloadData()
  └── 创建可视区域 Cell（4行×3列 + preFetchLine ≈ 15个）
      cellForRow(0~14)：
        card->setGame(name, version, modCount)   ← 文字立即显示
        game.iconId == 0 → 不调 setIcon          ← 显示默认图标（XML 已设）

时刻 2~5s：后台线程逐个加载
  每个游戏 → brls::sync → 主线程 applyMetadata(gameIdx, meta)：
    m_games[gameIdx].iconId = nvgCreateImageMem(...)  ← 创建纹理
    auto* cell = m_grid->getGridItemByIndex(gameIdx)
      在可视区域 → cell != nullptr → card->setIcon(iconId) ← 立即更新
      不在可视区域 → cell == nullptr → 不做 UI 操作
        → 下次用户滚到时 cellForRow(idx) 自然用 m_games[idx].iconId

时刻 ~5s：全部加载完 → 启用排序按钮
```

**纹理生命周期（关键）：**
- 纹理属于**数据层**（`m_games[idx].iconId`），不属于 Cell
- Cell 被回收时 `prepareForReuse()` 只重置 Image 控件显示，不删除纹理
- 200 个纹理全缓存在 GPU 中（~50MB），不释放
- 用户滚回去时 `cellForRow(idx)` 用已有的 `iconId` → 瞬间显示，无需重新加载

**优先级加载（必须保留）：**

场景：用户在异步加载期间翻页/快速滚动
```
后台从 idx 0 顺序加载，加载到 idx 25 时用户翻到 idx 36~47
如果顺序加载 → 用户要等后台慢慢加载到 idx 36 → 延迟感知
如果优先级加载 → 后台立刻切换到 idx 36 附近 → 可见区域优先更新
```

实现：后台线程代码（`startNacpLoader` 中的优先级查找逻辑）完全不变。
`m_focusedIndex` 的更新方式改为通过 RecyclingGrid 的焦点回调：

```cpp
// RecyclingGrid 新增 onChildFocusGained 回调
void RecyclingGrid::onChildFocusGained(View* directChild, View* focusedView) {
    ScrollingFrame::onChildFocusGained(directChild, focusedView);
    View* v = focusedView;
    while (v && !dynamic_cast<RecyclingGridItem*>(v)) v = v->getParent();
    if (v && m_focusChangeCallback)
        m_focusChangeCallback(static_cast<RecyclingGridItem*>(v)->getIndex());
}

// MainActivity 中：
m_grid->setFocusChangeCallback([this](size_t index) {
    m_focusedIndex.store(index);                                    // 后台线程优先级
    m_frame->setIndexText(fmt::format("{}/{}", index + 1, m_games.size())); // 索引显示
});
```

一个回调同时解决：1) 后台线程优先级加载  2) 底部索引显示

### 场景 2：商店（5000~10000 个游戏，分页加载）

**数据特点：**
- 数据从服务器分页获取，每页 ~50 条
- 图标为在线 URL，需异步下载
- 数据量不封顶

**RecyclingGrid 下的数据流：**

```
首次加载：
  请求第 1 页（50 条）→ m_storeGames 追加 50 条
  setDataSource(new StoreDataSource(...))
  → reloadData() → 创建可视区域 ~15 个 Cell
  → contentBox 高度 = 50/3 × rowHeight

用户滚到底部：
  itemsRecyclingLoop 检测 visibleMax + 1 >= getItemCount()
  → 触发 onNextPage → 请求第 2 页
  → 第 2 页返回 → m_storeGames 追加到 100 条
  → notifyDataChanged() → contentBox 高度增长，继续滚动

图标下载：
  cellForRow(idx) 时：
    已下载 → setIcon(iconId)
    未下载 → 默认图标 + 发起下载
    下载完 → brls::sync → getGridItemByIndex(idx) → setIcon
```

**纹理策略：**
- 商店图标用 LRU 缓存（TextureCache 容量限制）
- 超出容量自动淘汰远处的纹理
- 与主页策略不同，但差异被 DataSource 抽象层隔离

### 场景对比

| | 主页 | 商店 |
|---|---|---|
| 数据量 | ~200（固定） | 5000~10000（动态增长） |
| 数据来源 | 本地扫描 | 网络分页 |
| 图标来源 | Switch API（本地） | 在线下载 |
| 纹理策略 | 全缓存（~50MB） | LRU 缓存 |
| 分页 | 不需要 | onNextPage |
| 优先级加载 | 需要（异步期间用户可能翻页） | 可选 |
| RecyclingGrid 差异 | 无 | 无 |
| DataSource 差异 | GameCardDataSource | StoreDataSource |

**RecyclingGrid 组件本身零差异，场景差异完全由 DataSource 实现隔离。**

### 通用性

```
RecyclingGrid（通用引擎）
  ├── 主页 → GameCard + GameCardDataSource（spanCount=3）
  ├── 商店 → StoreCard + StoreDataSource（spanCount=3/4）
  └── Mod列表 → ModCard + ModListDataSource（spanCount=1，即单列列表）
```

每个页面只需写：
1. Cell 类（继承 RecyclingGridItem）→ 定义长什么样
2. DataSource 类（继承 RecyclingGridDataSource）→ 定义数据从哪来

---

## 五、实施步骤

### 步骤 1：移植 RecyclingGrid 框架组件

**新增文件：**
- `code/include/view/recyclingGrid.hpp`
- `code/src/view/recyclingGrid.cpp`

**来源：** 参考项目 `recycling_grid.cpp/hpp`，适配我们的项目。

**包含的类：**

```cpp
// 数据源接口（调用者实现）
class RecyclingGridDataSource {
    virtual size_t getItemCount() = 0;                                 // 数据总数
    virtual RecyclingGridItem* cellForRow(RecyclingGrid*, size_t) = 0;  // 按需创建/绑定
    virtual void onItemSelected(RecyclingGrid*, size_t) {}             // 点击回调
    virtual void clearData() {}                                        // 清空数据
};

// Cell 基类（GameCard 继承它）
class RecyclingGridItem : public brls::Box {
    size_t index;                           // 当前绑定的数据索引
    virtual void prepareForReuse() {}       // 被回收时重置状态
};

// 内容容器（焦点导航代理）
class RecyclingGridContentBox : public brls::Box {
    View* getNextFocus(...) override;       // 代理到 RecyclingGrid::getNextCellFocus
};

// 核心组件
class RecyclingGrid : public brls::ScrollingFrame {
    // 配置
    int spanCount;              // 列数（如 3）
    float estimatedRowHeight;   // 行高
    float estimatedRowSpace;    // 行间距
    int preFetchLine;           // 预取行数

    // Cell 管理
    void registerCell(string id, function<RecyclingGridItem*()> factory);
    RecyclingGridItem* dequeueReusableCell(string id);

    // 数据
    void setDataSource(RecyclingGridDataSource* source);
    void reloadData();
    void notifyDataChanged();

    // 导航
    void selectRowAt(size_t index, bool animated);
    View* getNextCellFocus(FocusDirection, View*);

    // 分页
    void onNextPage(function<void()> callback);

    // 查询
    RecyclingGridItem* getGridItemByIndex(size_t);

    // 焦点回调（新增，参考项目没有）
    void setFocusChangeCallback(function<void(size_t)> callback);
};
```

**核心机制（来自参考项目，无需重新实现）：**
- `itemsRecyclingLoop()`：在 `draw()` 中调用，根据可视区域自动增删 Cell
- `addCellAt(index)`：从回收池取 Cell → `cellForRow` 绑定数据 → detached 手动定位
- `removeCell(view)`：从 contentBox 移除 → 放入回收池
- `onLayout()`：首次布局完成时 `layouted = true` → `reloadData()`
- `reloadData()`：所有 Cell 回收 → 重新添加可视区域的 Cell

**需要适配的部分：**
- 移除 wiliwili 特有的依赖（`ButtonRefresh`、`hintImage`、`hintLabel`、`SkeletonCell`）
- 新增 `setFocusChangeCallback` + override `onChildFocusGained`
- 确保与我们的 borealis 版本 API 兼容
- 不需要复制 XML 文件（RecyclingGrid 在构造函数中编程式创建 contentBox）

**XML 注册（main.cpp 中添加）：**
```cpp
#include "view/recyclingGrid.hpp"
brls::Application::registerXMLView("RecyclingGrid", RecyclingGrid::create);
```
否则 XML 中无法使用 `<RecyclingGrid>` 标签。

---

### 步骤 2：适配 GameCard

**修改文件：**
- `code/include/view/gameCard.hpp`
- `code/src/view/gameCard.cpp`

**改动：**

```cpp
// 之前
class GameCard : public brls::Box {

// 之后
class GameCard : public RecyclingGridItem {
    void prepareForReuse() override {
        resetIcon();
        m_name->setText("");
        m_version->setText("");
        m_modCount->setText("");
    }
};
```

**删除的内容：**
- `setGame()` 中的 `setVisibility(VISIBLE)` 和 `setFocusable(true)`（RecyclingGridItem 构造函数已设 focusable，回收机制保证 Cell 始终 VISIBLE）
- `clear()` 方法（由 `prepareForReuse` 替代）

**修改的内容：**
- `GameCard::create()` 返回类型从 `brls::View*` 改为 `RecyclingGridItem*`（匹配 `registerCell` 的 `std::function<RecyclingGridItem*()>` 签名，否则编译失败）

**保留的内容：**
- `gameCard.xml`（UI 样式不变）
- `setGame()`、`setIcon()`、`resetIcon()`（数据绑定方法）
- `m_defaultIconId` + `setFreeTexture(false)` 机制（TextureCache 策略不变）

---

### 步骤 3：实现 GameCardDataSource

**新增文件：**
- `code/include/dataSource/gameCardDataSource.hpp`
- `code/src/dataSource/gameCardDataSource.cpp`

```cpp
class GameCardDataSource : public RecyclingGridDataSource {
    std::vector<GameInfo>& m_games;
    std::function<void(int)> m_clickCallback;

public:
    GameCardDataSource(std::vector<GameInfo>& games, std::function<void(int)> click)
        : m_games(games), m_clickCallback(click) {}

    size_t getItemCount() override {
        return m_games.size();
    }

    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto* card = static_cast<GameCard*>(grid->dequeueReusableCell("GameCard"));
        auto& game = m_games[index];
        card->setGame(game.displayName, game.version, game.modCount);
        if (game.iconId > 0) card->setIcon(game.iconId);
        // 不需要 else resetIcon —— prepareForReuse 已重置
        return card;
    }

    void onItemSelected(RecyclingGrid* grid, size_t index) override {
        if (m_clickCallback) m_clickCallback(index);
    }

    void clearData() override {}
};
```

---

### 步骤 4：改造 MainActivity

**修改文件：**
- `code/include/activity/main_activity.hpp`
- `code/src/activity/main_activity.cpp`
- `resources/xml/activity/main.xml`（GridPage 标签 → RecyclingGrid）

**hpp 改动：**
```cpp
// 之前
#include "view/gridPage.hpp"
BRLS_BIND(GridPage, m_gridPage, "main/gridPage");

// 之后
#include "view/recyclingGrid.hpp"
BRLS_BIND(RecyclingGrid, m_grid, "main/grid");
```

**setupGridPage 改动：**
```cpp
void MainActivity::setupGridPage() {
    m_grid->registerCell("GameCard", GameCard::create);

    auto* ds = new GameCardDataSource(m_games, [this](int index) {
        auto& game = m_games[index];
        brls::Application::pushActivity(new ModManager(game.dirPath, game.displayName));
    });
    m_grid->setDataSource(ds);

    m_grid->setFocusChangeCallback([this](size_t index) {
        m_focusedIndex.store(index);
        m_frame->setIndexText(fmt::format("{}/{}", index + 1, m_games.size()));
    });
}
```

**applyMetadata 改动：**
```cpp
void MainActivity::applyMetadata(int gameIdx, const GameMetadata& meta) {
    // ... 更新 m_games[gameIdx] 数据（不变）

    // 刷新可见 Cell
    auto* cell = m_grid->getGridItemByIndex(gameIdx);
    if (cell) {
        auto* card = static_cast<GameCard*>(cell);
        card->setGame(m_games[gameIdx].displayName, m_games[gameIdx].version, m_games[gameIdx].modCount);
        if (m_games[gameIdx].iconId > 0) card->setIcon(m_games[gameIdx].iconId);
    }
    // 不在可视区域 → 不做 UI 操作，下次 cellForRow 自然用新数据
}
```

**toggleSort 改动：**
```cpp
void MainActivity::toggleSort() {
    m_sortAsc = !m_sortAsc;
    strSort::sortAZ(m_games, ...);
    m_grid->reloadData();  // 回收所有 Cell，重新绑定可视区域
}
```

**flipScreen 改动：**

注意：`selectRowAt` 只设置滚动位置 + `setLastFocusedView`，不调用 `giveFocus`。
翻页需要**立即**切换焦点 + 1ms 动画 trick 防高亮闪烁。

```cpp
void MainActivity::flipScreen(int direction) {
    auto* focus = brls::Application::getCurrentFocus();
    if (!focus) return;
    while (focus && !dynamic_cast<RecyclingGridItem*>(focus))
        focus = focus->getParent();
    if (!focus) return;
    size_t idx = static_cast<RecyclingGridItem*>(focus)->getIndex();

    int rowsPerScreen = std::max(1, (int)(m_grid->getHeight() / m_grid->estimatedRowHeight));
    int target = idx + direction * m_grid->spanCount * rowsPerScreen;
    target = std::clamp(target, 0, (int)m_grid->getDataSource()->getItemCount() - 1);
    if ((size_t)target == idx) return;

    // selectRowAt 确保 target Cell 在 contentBox 中
    m_grid->selectRowAt(target, false);
    auto* cell = m_grid->getGridItemByIndex(target);
    if (!cell) return;

    // 1ms 动画 trick 防高亮闪烁（与当前 GridPage 一致）
    auto style = brls::Application::getStyle();
    float saved = style["brls/animations/highlight"];
    style.addMetric("brls/animations/highlight", 1.0f);
    brls::Application::giveFocus(cell);
    style.addMetric("brls/animations/highlight", saved);
}
```

**LB/RB 注册移到 RecyclingGrid 内部或 setupGridPage 中：**
```cpp
m_grid->registerAction("上翻", brls::BUTTON_LB, [this](...) {
    flipScreen(-1);
    return true;
}, true, true);
m_grid->registerAction("下翻", brls::BUTTON_RB, [this](...) {
    flipScreen(1);
    return true;
}, true, true);
```

**startNacpLoader：后台线程代码完全不变。**
`m_focusedIndex` 通过 `setFocusChangeCallback` 在焦点变化时更新。

**showEmptyHint 改动（搜索替换）：**
`m_gridPage` → `m_grid`

---

### 步骤 5：清理废弃代码

**删除文件：**
- `code/include/view/gridPage.hpp`
- `code/src/view/gridPage.cpp`
- `resources/xml/view/gridPage.xml`
- `code/include/utils/indexUpdate.hpp`（确认无其他使用者后）

**CMakeLists.txt：**
- 移除 `gridPage.cpp`
- 添加 `recyclingGrid.cpp`、`gameCardDataSource.cpp`

---

## 六、解决的问题清单

| 问题 | 如何解决 |
|------|---------|
| 悬空指针（setItemCount） | Cell 不删除，放入回收池 |
| 布局时序（giveFocus 过早） | `layouted` 标志，`onLayout` 后才操作 |
| 内存 O(N) | 只创建可视区域的 ~15 个 Cell |
| 引用计数泄漏 | Cell 复用不重复创建，count 稳定 |
| 全量绑定卡顿 | 按需绑定（cellForRow） |
| 不支持分页 | `onNextPage` 回调 |

## 七、不变的部分

- **UI 样式**：`gameCard.xml` 不改
- **异步加载逻辑**：`startNacpLoader` 后台线程代码完全不变
- **优先级加载**：`m_focusedIndex` + 就近查找逻辑不变，只改更新来源（焦点回调）
- **排序逻辑**：`toggleSort` 不变，只改刷新方式
- **TextureCache 策略**：`setFreeTexture(false)` + `innerSetImage` 不变
- **其他页面**：ModManager 等完全不受影响

## 八、风险

| 风险 | 缓解 |
|------|------|
| borealis 版本差异 | 移植时逐个验证 API 兼容性 |
| 参考项目代码可能有 bug | wiliwili 是成熟项目，经过大量用户验证 |
| flipScreen 动画交互 | selectRowAt 已有动画支持 |
| 异步加载时 Cell 可能已被回收 | `getGridItemByIndex` 返回 nullptr 时跳过，下次 cellForRow 自然绑定 |
