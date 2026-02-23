# 收藏功能实现计划

## 已有基础

| 模块 | 状态 |
|------|------|
| `GameInfo.isFavorite` | ✅ 已有字段 |
| `GameScanner` 读 JSON `favorite` | ✅ 已有 |
| `strSort::sortAZ` 收藏分组排序 | ✅ 已有 |
| 菜单项 "收藏游戏" 动态标题 | ✅ 已有（缺 action） |
| `like.png` 爱心图片 | ✅ 已有 |
| `JsonFile.setBool` | ✅ 已有 |

## 需要修改的文件（共 4 个）

---

### 1. `gameCard.xml` — 爱心图标叠加

图标区域改为 Box 包裹，爱心绝对定位在右上角。

```xml
<!-- 改前 -->
<brls:Image
    id="gameCard/icon"
    width="120"
    height="120"
    marginRight="20"
    scalingType="fit"
    cornerRadius="8"
    image="@res/img/game/defaultIcon.jpg" />

<!-- 改后 -->
<brls:Box
    width="120"
    height="120"
    marginRight="20">

    <brls:Image
        id="gameCard/icon"
        width="120"
        height="120"
        scalingType="fit"
        cornerRadius="8"
        image="@res/img/game/defaultIcon.jpg" />

    <brls:Image
        id="gameCard/like"
        width="25"
        height="25"
        positionType="absolute"
        positionTop="5"
        positionRight="5"
        scalingType="fit"
        visibility="gone"
        image="@res/img/game/like.png" />

</brls:Box>
```

---

### 2. `gameCard.hpp` + `gameCard.cpp` — 新增 setFavorite

```cpp
// gameCard.hpp 新增：
BRLS_BIND(brls::Image, m_like, "gameCard/like");  // 爱心图标
void setFavorite(bool favorite);                    // 显示/隐藏爱心

// gameCard.cpp 新增：
void GameCard::setFavorite(bool favorite) {
    m_like->setVisibility(favorite ? brls::Visibility::VISIBLE : brls::Visibility::GONE);
}

// prepareForReuse 新增一行：
m_like->setVisibility(brls::Visibility::GONE);
```

---

### 3. `gameCardDS.hpp` — cellForRow 绑定收藏状态

```cpp
// cellForRow 中，setIcon 之后新增：
card->setFavorite(game.isFavorite);
```

---

### 4. `home.cpp` — 菜单 action + 排序 + 焦点恢复

```cpp
// setupMenu 中，"收藏游戏" 选项追加 .action：
MenuItemConfig{"收藏游戏", "将当前游戏加入/取消收藏，收藏的游戏会优先显示在列表顶部"}
    .title([this]{ return m_games[m_focusedIndex.load()].isFavorite ? "取消收藏" : "收藏游戏"; })
    .action([this]{
        int idx = m_focusedIndex.load();
        auto& game = m_games[idx];

        // 1. 翻转收藏状态
        game.isFavorite = !game.isFavorite;

        // 2. 写入 JSON 并保存
        std::string appIdKey = format::appIdHex(game.appId);
        m_jsonCache.setBool(appIdKey, "favorite", game.isFavorite);
        m_jsonCache.save();

        // 3. 记住当前游戏 appId，排序后用它找新位置
        uint64_t targetAppId = game.appId;

        // 4. 重新排序
        strSort::sortAZ(m_games, &GameInfo::displayName, &GameInfo::isFavorite, m_sortAsc);

        // 5. 找到目标游戏的新索引
        int newIdx = 0;
        for (int i = 0; i < (int)m_games.size(); i++) {
            if (m_games[i].appId == targetAppId) { newIdx = i; break; }
        }

        // 6. 刷新列表 + 焦点恢复到目标游戏
        m_grid->setDefaultCellFocus(newIdx);
        m_grid->reloadData();
        auto* cell = m_grid->getGridItemByIndex(newIdx);
        if (cell) brls::Application::giveFocus(cell);
    }),
```

---

## 执行顺序

1. 改 `gameCard.xml`（爱心叠加层）
2. 改 `gameCard.hpp` + `gameCard.cpp`（setFavorite 方法）
3. 改 `gameCardDS.hpp`（cellForRow 绑定）
4. 改 `home.cpp`（菜单 action 逻辑）