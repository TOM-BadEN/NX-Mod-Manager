# GridPage 通用化重构

## 背景

当前 GridPage 写死了 GameCard 类型（XML 中 `<GameCard>`，代码中 `GameCard*`），
未来有多个使用相同网格翻页布局但不同卡片类型的场景（如添加游戏页面）。

### 目标

GridPage 不绑定任何具体卡片类型，通过参数化行列数 + 工厂函数 + 回调实现通用化。

### 设计方案

#### 核心接口

```cpp
class GridPage : public brls::Box {
public:
    GridPage();

    // ── 初始化 ──
    // 创建 rows×cols 个卡片，添加到自动创建的行容器中
    void setup(int rows, int cols, std::function<brls::Box*()> factory);

    // ── 数据 ──
    void setItemCount(int count);       // 设置数据总数
    void reloadData();                  // 数据变更后重刷当前页

    // ── 回调 ──
    // refresh: 将第 globalIndex 条数据填入 slot
    // clear:   清空 slot（超出数据范围时调用）
    void setRefreshCallback(
        std::function<void(brls::Box*, int)> refresh,
        std::function<void(brls::Box*)> clear
    );
    void setClickCallback(std::function<void(int)> callback);
    void setIndexChangeCallback(std::function<void(int, int)> callback);

    // ── 翻页 ──
    void nextPage();
    void prevPage();
    int getCurrentPage() const;

    // ── 异步更新 ──
    // 更新指定全局索引的槽位（如果在当前页则调用 refreshCallback）
    void updateSlot(int globalIndex);

    // ── 焦点 ──
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

    static brls::View* create();

private:
    int m_rows = 0;                     // 行数
    int m_cols = 0;                     // 列数
    int m_cardsPerPage = 0;             // rows * cols
    int m_totalItems = 0;               // 数据总数
    int m_currentPage = 0;

    std::vector<brls::Box*> m_cards;    // 通用 Box*，不绑定具体类型
    IndexUpdate m_indexUpdate;

    std::function<void(brls::Box*, int)> m_refreshCallback;
    std::function<void(brls::Box*)> m_clearCallback;
    std::function<void(int)> m_clickCallback;

    int getTotalPages();
    void refreshPage();
    void flipPage(int delta);
    void focusCard(int cardIndex);
    void fixFocusAfterFlip();
    int findFocusedCardIndex();
    bool isCardVisible(int index);
    int findVisibleCard(int start, int step);
};
```

#### XML（gridPage.xml）

只保留外层容器样式，不含行容器和卡片：

```xml
<brls:Box
    width="auto"
    height="auto"
    axis="column"
    grow="1"
    paddingLeft="40"
    paddingRight="40"
    paddingTop="10"
    paddingBottom="10"/>
```

行容器和卡片由 `setup()` 代码动态创建。

#### setup() 实现逻辑

```
setup(rows, cols, factory):
    m_rows = rows
    m_cols = cols
    m_cardsPerPage = rows * cols
    m_cards.resize(m_cardsPerPage)

    for r in 0..rows-1:
        row = new brls::Box()
        row->setAxis(ROW)
        row->setGrow(1)
        row->setJustifyContent(flexStart)
        row->setAlignItems(center)
        addView(row)

        for c in 0..cols-1:
            i = r * cols + c
            m_cards[i] = factory()
            m_cards[i]->setWidth(0)       // 由 flex 分配
            m_cards[i]->setGrow(1)
            m_cards[i]->setShrink(1)
            m_cards[i]->setMargins(9,9,9,9)
            row->addView(m_cards[i])

            // 注册焦点事件 → 更新索引
            m_cards[i]->getFocusEvent()->subscribe([this, i](View*) {
                if (m_totalItems > 0)
                    m_indexUpdate.update(m_currentPage * m_cardsPerPage + i, m_totalItems);
            })

            // 注册点击事件
            m_cards[i]->registerClickAction([this, i](View*) {
                int globalIndex = m_currentPage * m_cardsPerPage + i;
                if (m_clickCallback && isCardVisible(i)) m_clickCallback(globalIndex);
                return true;
            })
```

#### refreshPage() 变化

```
旧：m_cards[i]->setGame(game.displayName, game.version, game.modCount);
新：m_refreshCallback(m_cards[i], dataIndex);

旧：m_cards[i]->clear();
新：m_clearCallback(m_cards[i]);
```

#### 焦点导航变化

getNextFocus 中所有用到 `3`（列数）的地方改为 `m_cols`，
所有用到 `CARDS_PER_PAGE`/`9` 的地方改为 `m_cardsPerPage`。

具体涉及：
- UP/DOWN 跳行时 `± m_cols`（原来写死 ±3）
- LEFT/RIGHT 换行时 `% m_cols`（原来写死 %3）
- 翻页边界判断用 `m_cardsPerPage`

#### updateSlot() 变化（原 updateCard）

```cpp
void GridPage::updateSlot(int globalIndex) {
    int cardIndex = globalIndex - m_currentPage * m_cardsPerPage;
    if (cardIndex < 0 || cardIndex >= m_cardsPerPage) return;
    if (m_refreshCallback) m_refreshCallback(m_cards[cardIndex], globalIndex);
}
```

### 调用方变化（MainActivity）

```cpp
// ── 旧代码 ──
m_gridPage->setGameList(m_games);

// ── 新代码 ──
m_gridPage->setup(3, 3, []() { return new GameCard(); });
m_gridPage->setItemCount(m_games.size());
m_gridPage->setRefreshCallback(
    [this](brls::Box* slot, int index) {
        auto* card = static_cast<GameCard*>(slot);
        auto& game = m_games[index];
        card->setGame(game.displayName, game.version, game.modCount);
        if (game.iconId > 0) card->setIcon(game.iconId);
    },
    [](brls::Box* slot) {
        static_cast<GameCard*>(slot)->clear();
    }
);

// updateCard 变化
m_gridPage->updateSlot(gameIdx);   // 原来是 m_gridPage->updateCard(gameIdx);
```

### 改动文件清单

| 文件 | 改动说明 |
|---|---|
| `resources/xml/view/gridPage.xml` | 删除所有行容器和 GameCard，只保留外层 Box 样式 |
| `code/include/view/gridPage.hpp` | GameCard*/GameInfo* → Box*/vector, 常量→变量, 新增 setup/setItemCount/setRefreshCallback, 删除 setGameList/updateCard |
| `code/src/view/gridPage.cpp` | 构造函数精简, setup() 动态创建, refreshPage 用回调, 焦点导航用 m_cols/m_cardsPerPage |
| `code/include/activity/main_activity.hpp` | 去掉 #include "view/gameCard.hpp"（移到 cpp） |
| `code/src/activity/main_activity.cpp` | setupGridPage() 适配新接口, updateCard→updateSlot, include gameCard.hpp |

### 不变的部分

- GameCard 组件本身（xml/hpp/cpp）完全不变
- 翻页逻辑（nextPage/prevPage/flipPage）结构不变，只是常量变量化
- 焦点导航逻辑结构不变，只是 3→m_cols, 9→m_cardsPerPage
- LB/RB 按键注册不变
- IndexUpdate 使用方式不变

---

## 实施步骤

### 第 1 步：修改 gridPage.xml

精简 XML，删除所有行容器和 `<GameCard>`，只保留外层 Box 样式属性。

**操作**：
- 删除 3 个 `<brls:Box>` 行容器及其内部 9 个 `<GameCard>`
- 保留外层 `<brls:Box>` 的 axis/grow/padding 等样式
- 改为自闭合标签（无子元素）

**验证**：XML 只剩一个空 Box 容器

---

### 第 2 步：重写 gridPage.hpp

去掉所有对 GameCard/GameInfo 的依赖，改为通用接口。

**操作**：
1. 删除 `#include "view/gameCard.hpp"` 和 `#include "common/gameInfo.hpp"`
2. 删除 `static constexpr int CARDS_PER_PAGE = 9`
3. `GameCard* m_cards[CARDS_PER_PAGE]` → `std::vector<brls::Box*> m_cards`
4. `std::vector<GameInfo>* m_games` → `int m_totalItems`
5. 新增成员：`int m_rows, m_cols, m_cardsPerPage`
6. 新增回调成员：`m_refreshCallback, m_clearCallback, m_clickCallback`
7. 删除方法：`setGameList()`, `updateCard()`
8. 新增方法：`setup()`, `setItemCount()`, `setRefreshCallback()`, `updateSlot()`
9. `setClickCallback()` 和 `setIndexChangeCallback()` 保留不变

**验证**：hpp 不再 include 任何业务头文件（gameCard/gameInfo）

---

### 第 3 步：重写 gridPage.cpp

核心改动最多的文件，按函数逐个修改。

**3.1 构造函数**
- 保留 `inflateFromXMLRes("xml/view/gridPage.xml")`
- 保留 LB/RB 翻页注册（setup 之前按翻页不会崩溃，因为 m_cardsPerPage=0 时 getTotalPages 返回 1）
- 删除 for 循环（获取卡片引用、注册焦点/点击 → 移到 setup）

**3.2 新增 setup(rows, cols, factory)**
- 保存 m_rows, m_cols, m_cardsPerPage
- 创建 rows 个行容器（brls::Box, axis=ROW, grow=1, alignItems=center）
- 每行创建 cols 个卡片（factory(), width=0, grow=1, shrink=1, margin=9）
- 注册焦点事件：用 m_totalItems 替代 m_games->size()
- 注册点击事件（registerClickAction）

**3.3 setItemCount(count)**
- 只设 `m_totalItems = count`，不重置页码，不调 refreshPage
- 首次初始化由调用方显式调 refreshPage/reloadData

**3.4 refreshPage()**
- 循环 `m_cardsPerPage` 次（原来写死 CARDS_PER_PAGE）
- `m_refreshCallback(m_cards[i], dataIndex)` 替代 `m_cards[i]->setGame(...)`
- `m_clearCallback(m_cards[i])` 替代 `m_cards[i]->clear()`
- 加空指针检查：`if (!m_refreshCallback || !m_clearCallback) return;`

**3.5 getNextFocus()** — 硬编码替换明细
- `cardIndex / 3` → `cardIndex / m_cols`
- `cardIndex % 3` → `cardIndex % m_cols`
- RIGHT: `col < 2` → `col < m_cols - 1`
- RIGHT: `row < 2` → `row < m_rows - 1`
- RIGHT: `(row + 1) * 3` → `(row + 1) * m_cols`
- LEFT: `(row - 1) * 3 + 2` → `(row - 1) * m_cols + (m_cols - 1)`
- LEFT: `targetIndex = 8` → `targetIndex = m_cardsPerPage - 1`
- DOWN: `cardIndex + 3` → `cardIndex + m_cols`
- DOWN: `row < 2` → `row < m_rows - 1`
- UP: `cardIndex - 3` → `cardIndex - m_cols`
- UP: `6 + col` → `(m_rows - 1) * m_cols + col`
- 末尾: `m_games->size()` → `m_totalItems`
- 所有 `CARDS_PER_PAGE` → `m_cardsPerPage`
- 逻辑结构完全不变

**3.6 getTotalPages()**
- `if (!m_games || m_games->empty()) return 1` → `if (m_totalItems <= 0) return 1`
- `m_games->size()` → `m_totalItems`
- `CARDS_PER_PAGE` → `m_cardsPerPage`

**3.7 focusCard()**
- `m_games->size()` → `m_totalItems`
- `CARDS_PER_PAGE` → `m_cardsPerPage`

**3.8 updateSlot(globalIndex)**（原 updateCard）
- `m_refreshCallback(m_cards[cardIndex], globalIndex)` 替代直接调用 setGame/setIcon
- `CARDS_PER_PAGE` → `m_cardsPerPage`

**3.9 其他函数**
- `flipPage`, `fixFocusAfterFlip`: 无硬编码，不变
- `findFocusedCardIndex`, `isCardVisible`, `findVisibleCard`: `CARDS_PER_PAGE` → `m_cardsPerPage`
- `nextPage`, `prevPage`: `CARDS_PER_PAGE` → `m_cardsPerPage`
- `reloadData()` 不变，内部调 refreshPage()
- `setClickCallback()`, `setIndexChangeCallback()` 保留
- 删除 `setGameList()`

**验证**：gridPage.cpp 不再 include gameCard.hpp / gameInfo.hpp

---

### 第 4 步：修改 main_activity.hpp

**操作**：
- 删除 `#include "view/gameCard.hpp"`（移到 cpp 中）
- 删除 `#include "common/gameInfo.hpp"`（如果只在 hpp 中被 GridPage 间接引用的话——实际 main_activity.hpp 自己也用 GameInfo，所以保留）

**验证**：编译不因缺少头文件报错

---

### 第 5 步：修改 main_activity.cpp

**操作**：
1. 添加 `#include "view/gameCard.hpp"`
2. `setupGridPage()` 中：
   - `m_gridPage->setGameList(m_games)` → 四步调用：
     ```cpp
     m_gridPage->setup(3, 3, []() { return new GameCard(); });
     m_gridPage->setItemCount(m_games.size());
     m_gridPage->setRefreshCallback(refresh, clear);
     m_gridPage->reloadData();  // 首次刷新
     ```
   - 索引回调和点击回调保留不变
3. `applyMetadata()` 中：
   - `m_gridPage->updateCard(gameIdx)` → `m_gridPage->updateSlot(gameIdx)`
4. `toggleSort()` 中：
   - `m_gridPage->reloadData()` 之前加 `m_gridPage->setItemCount(m_games.size())`（数据量可能因排序过滤而变化，虽然目前不变，但保持一致性）

**验证**：主页功能和重构前完全一致（翻页、焦点、点击跳转、异步加载图标）

---

### 第 6 步：编译验证

**操作**：
1. 全量编译，修复编译错误
2. 部署到 Switch 测试：
   - 主页 3x3 网格正常显示
   - LB/RB 翻页正常
   - 上下左右焦点导航正常
   - A 键点击跳转 ModManager 正常
   - 异步图标加载正常更新到卡片
   - Y 键排序后页面刷新正常
   - 空列表提示正常
