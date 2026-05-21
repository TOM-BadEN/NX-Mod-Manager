# 移除最后一个模组 — 实现计划书

## 一、目标

允许用户移除 ModList 中的最后一个模组。移除后自动删除该游戏并返回主页。

当前行为：`mods().size() <= 1` 时"移除模组"菜单项被禁用。
目标行为：始终允许移除（仅 `isInstalled` 和 `sizeLoading` 阻止），最后一个模组走单独流程。

---

## 二、流程

### 普通移除（非最后一个）

保持现有 `removeModFromList()` 不变。

### 最后一个移除（新方法 `removeLastModFromList()`）

1. 弹出专用确认框（三按钮：取消、取消、确认移除）
2. 提示词说明：移除后该游戏将从列表中删除，MOD 移至中转站
3. 用户确认后：
   - 设置 `m_gameManager.setPendingRemove(game.appId)`
   - 直接 `popActivity` 返回主页
   - **不调 `removeModFromModList`**，由 `removeGame` 统一处理移动 mod + 清理
4. Home `onResume` 检测 `consumePendingRemove()`，执行 `removeGame` + 刷新 grid

---

## 三、文件改动

### 3.1 gameManager.hpp

新增 private 成员：
```cpp
uint64_t m_pendingRemoveAppId = 0;
```

新增 public 方法：
```cpp
void setPendingRemove(uint64_t appId);
uint64_t consumePendingRemove();
```

---

### 3.2 gameManager.cpp

```cpp
void GameManager::setPendingRemove(uint64_t appId) {
    m_pendingRemoveAppId = appId;
}

uint64_t GameManager::consumePendingRemove() {
    uint64_t id = m_pendingRemoveAppId;
    m_pendingRemoveAppId = 0;
    return id;
}
```

---

### 3.3 modList.hpp

新增 private 方法声明：
```cpp
void removeLastModFromList();
```

---

### 3.4 modList.cpp — setupManageMenu()

移除 `size() <= 1` 限制，改 action 为根据数量分发：

```cpp
auto& removeModItem = m_manageMenu.addItem(brls::getStr("modList/removeMod"), brls::getStr("modList/removeModDesc"));
removeModItem.setDisabled([this]{
    if (m_modManager.mods().empty()) return true;
    auto idx = m_focusedIndex;
    if (idx >= static_cast<int>(m_modManager.mods().size())) return true;
    if (m_modManager.mods()[idx].isInstalled) return true;
    return m_sizeLoading;
});
removeModItem.setAction([this]{
    if (m_modManager.mods().size() == 1) removeLastModFromList();
    else removeModFromList();
});
```

---

### 3.5 modList.cpp — 新增 removeLastModFromList()

```cpp
void ModList::removeLastModFromList() {
    auto onConfirm = [this] {
        auto& game = m_gameManager.games()[m_gameIndex];
        m_gameManager.setPendingRemove(game.appId);
        CustomDialog::close([] {
            brls::Application::popActivity();
        });
    };

    CustomDialog::show(brls::getStr("modList/removeLastModConfirm"), {
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/removeModBtn"), onConfirm},
    });
}
```

---

### 3.6 modList.cpp — removeModFromList() 修改

删除内部 `if (m_modManager.mods().empty())` 分支（不再可能走到）：

```cpp
void ModList::removeModFromList() {
    int idx = m_focusedIndex;

    auto onConfirm = [this, idx] {
        m_modManager.removeModFromModList(idx);

        auto& game = m_gameManager.games()[m_gameIndex];
        game.modCount = std::to_string(m_modManager.mods().size());
        m_gameManager.setPendingFocus(game.appId);

        int newFocus = std::min(idx, static_cast<int>(m_modManager.mods().size()) - 1);
        CustomDialog::close([this, newFocus] { refreshAndFocus(newFocus); });
    };

    CustomDialog::show(brls::getStr("modList/removeModConfirm"), {
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/cancel"), [] { CustomDialog::close(); }},
        {brls::getStr("modList/removeModBtn"), onConfirm},
    });
}
```

---

### 3.7 home.cpp — onResume() 修改

在现有 `consumePendingFocus` 之前插入 pendingRemove 检查：

```cpp
void Home::onResume() {
    // 检查是否需要移除游戏（从 ModList 移除最后一个 mod 触发）
    uint64_t removeAppId = m_gameManager.consumePendingRemove();
    if (removeAppId != 0) {
        int removeIdx = m_gameManager.findByAppId(removeAppId);
        m_gameManager.removeGame(removeIdx);
        if (m_gameManager.games().empty()) {
            showEmptyHint();
        } else {
            int newFocus = std::min(removeIdx, static_cast<int>(m_gameManager.games().size()) - 1);
            m_grid->deferReload(newFocus);
            m_focusedIndex = newFocus;
        }
        return;
    }

    // 原有 pendingFocus 逻辑
    uint64_t appId = m_gameManager.consumePendingFocus();
    if (appId == 0) return;
    m_gameManager.sort(m_sortMode, m_sortAsc);
    int newIdx = m_gameManager.findByAppId(appId);
    if (m_grid->getVisibility() == brls::Visibility::GONE) {
        m_grid->setVisibility(brls::Visibility::VISIBLE);
        m_noModHint->setVisibility(brls::Visibility::GONE);
    }
    m_grid->deferReload(newIdx);
    m_focusedIndex = newIdx;
}
```

---

### 3.8 i18n — 三语言新增 removeLastModConfirm

**zh-Hans/modList.json**：
```json
"removeLastModConfirm": "当前模组为该游戏的最后一项，移除后将同时删除该游戏项目。模组将移至中转站，可重新添加。"
```

**zh-Hant/modList.json**：
```json
"removeLastModConfirm": "當前模組為該遊戲的最後一項，移除後將同時刪除該遊戲項目。模組將移至中轉站，可重新新增。"
```

**en-US/modList.json**：
```json
"removeLastModConfirm": "This is the last mod for this game. Removing it will also delete the game project. The mod will be moved to the transit station and can be re-added."
```

放置位置：`removeModConfirm` 下方。

---

## 四、安全性说明

- `removeLastModFromList` 不做任何数据操作，只设标记 + pop
- `m_gameManager.games()[m_gameIndex]` 在 pop 前仍有效（game 未被 erase，mod 未被移动）
- 真正的 `removeGame` 延迟到 Home `onResume` 执行，此时 ModList 已销毁
- `removeGame` 统一处理：移动 mod 到中转站 + 清下载记录 + 删游戏目录 + 清 JSON + erase

---

## 五、不需要改的

- ModManager::removeModFromModList（逻辑不变）
- GameManager::removeGame（逻辑不变）
- Home::removeGame（仅 Home 菜单自己调用，不受影响）
- modList.hpp 中其他菜单和方法
