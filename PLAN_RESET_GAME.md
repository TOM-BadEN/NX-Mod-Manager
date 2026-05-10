# 重置状态 - 实现计划

## 功能概述

在 Home 页面「管理游戏」子菜单中新增「重置状态」选项。
点击后弹出长按确认对话框，长按确认后执行状态清理流程，清理期间显示进度对话框（不可中断、禁用 Home 键）。

---

## 清理范围

| # | 数据 | 位置 | 操作 |
|---|---|---|---|
| 1 | 每个 MOD 的安装状态 | `{gameDirPath}/modInfo.json` → `installed` | 删除 key |
| 2 | 每个 MOD 的商店关联 | `{gameDirPath}/modInfo.json` → `modID` | 删除 key |
| 3 | 每个游戏的引用计数 | `{gameDirPath}/modFileRefCount.json` | 删除文件 |
| 4 | 游戏安装标记 | `/mods2/gameInfo.json` → `hasInstalledMod` | 删除 key |
| 5 | 游戏禁用标记 | `/mods2/gameInfo.json` → `modsDisabled` | 删除 key |
| 6 | 商店下载记录 | `/config/NX-Mod-Manager/modShop/downloadRecord.txt` | 删除文件 |

**不动**：MOD 文件本体、中转站、atmosphere 目录、用户个性化数据（displayName、favorite、gameName、version）、Settings。

---

## UI 流程

```
主菜单(X键) → 管理游戏 → 重置游戏
  → LongPressDialog（提示词待定，按钮文本待定，3秒长按）
    → 长按完成：
      1. HomeBtnBlock::disable()
      2. ProgressDialog::show("正在重置中...", {}, nullptr)
         - 主文本: "正在重置中..."
         - leftText: 当前游戏显示名（优先 displayName）
         - rightText: "3 / 12"（当前序号 / 总游戏数）
         - mainProgress: 百分比
      3. 完成后:
         HomeBtnBlock::enable()
         CustomDialog 显示完成提示
```

---

## 代码改动

### 1. `code/include/core/gameManager.hpp`

新增声明：

```cpp
/**
 * @brief 重置所有游戏的安装状态（不删除 MOD 文件）
 * @param onProgress 进度回调(当前序号, 总数, 游戏显示名)
 */
void resetState(std::function<void(int, int, const std::string&)> onProgress);
```

### 2. `code/src/core/gameManager.cpp`

新增实现：

```cpp
void GameManager::resetState(std::function<void(int, int, const std::string&)> onProgress) {
    int total = static_cast<int>(m_games.size());

    for (int i = 0; i < total; i++) {
        auto& game = m_games[i];
        if (onProgress) onProgress(i + 1, total, game.displayName);

        // 清理 modInfo.json: 删 installed、modID
        JsonFile modJson;
        if (modJson.load(game.dirPath + config::modInfoFile)) {
            auto modDirs = fs::listSubDirs(game.dirPath);
            for (const auto& dir : modDirs) {
                modJson.removeKey(dir, "installed");
                modJson.removeKey(dir, "modID");
            }
            modJson.save();
        }

        // 删除 modFileRefCount.json
        fs::deleteFile(game.dirPath + config::refCountFile);

        // 清理 gameInfo.json 中的状态 key
        std::string key = format::appIdHex(game.appId);
        m_jsonCache.removeKey(key, "hasInstalledMod");
        m_jsonCache.removeKey(key, "modsDisabled");

        // 同步内存
        game.hasInstalledMod = false;
        game.isModsDisabled = false;
    }
    m_jsonCache.save();

    // 清理商店下载记录
    fs::deleteFile(config::downloadRecordPath);
}
```

### 3. `code/include/activity/home.hpp`

新增成员和声明：

```cpp
util::AsyncTask m_resetTask;

/** @brief 重置状态流程 */
void resetState();
```

### 4. `code/src/activity/home.cpp`

#### 4a. `setupGameManageMenu()` 中新增菜单项（在 `transitItem` 之前）

```cpp
auto& resetItem = m_gameManageMenu.addItem(brls::getStr("home/resetState"), brls::getStr("home/resetStateDesc"));
resetItem.setAction([this]{ resetState(); });
```

#### 4b. 新增 `Home::resetGame()` 实现

参考 `modList.cpp` forceClean 写法：

```cpp
void Home::resetState() {
    auto onConfirm = [this] {
        HomeBtnBlock::disable();
        ProgressDialog::show(brls::getStr("home/resetting"), {}, nullptr);

        m_resetTask = util::async([this](std::stop_token) {
            m_gameManager.resetState([](int current, int total, const std::string& name) {
                brls::sync([=] {
                    ProgressDialog::setLeftText(name);
                    ProgressDialog::setRightText(std::to_string(current) + " / " + std::to_string(total));
                    if (total > 0) ProgressDialog::setMainProgress(current * 100.0f / total);
                });
            });

            for (int i = 0; i < 30; i++) svcSleepThread(10000000ULL);  // 300ms 防闪烁

            brls::sync([] {
                HomeBtnBlock::enable();
                CustomDialog::show(brls::getStr("home/resetComplete"), {
                    {brls::getStr("home/ok"), [] { CustomDialog::close(); }}
                });
            });
        });
    };

    LongPressDialog::show(
        brls::getStr("home/resetStateNotice"),
        brls::getStr("home/resetStateBtn"),
        3.0f,
        onConfirm
    );
}
```

#### 4c. 新增 include

```cpp
#include "core/homeBtnBlock.hpp"  // 如果 home.cpp 尚未 include
```

### 5. i18n 字符串（待填）

| key | 暂定值 |
|---|---|
| `home/resetState` | "重置状态" |
| `home/resetStateDesc` | "请仔细阅读本说明，仅在确实需要重建模组环境时使用，否则数据彻底混乱" |
| `home/resetStateNotice` | "请仔细阅读本说明，仅在确实需要重建模组环境时使用，否则数据彻底混乱。\n\n当 \\mods2 文件夹内保存的模组状态数据与 SD 卡中实际已安装的模组文件完全不一致时，可使用此功能清除所有状态数据，重新建立干净的模组环境。\n\n仅在下述两种情况下使用：\n\n- 完全重装大气层整合包\n即，先删除原有大气层文件后进行的重装方式，对于覆盖安装请勿使用。\n\n- 使用他人分享的 \\mods2 文件\n仅在自身的 \\mods2 文件夹完全为空时使用。" |
| `home/resetStateBtn` | "长按确认" |
| `home/resetting` | "正在重置中..." |
| `home/resetComplete` | "已完成所有游戏的状态重置！" |

---

## 依赖检查

| 依赖 | 状态 |
|---|---|
| `HomeBtnBlock` | home.cpp 未 include，需新增 `#include "core/homeBtnBlock.hpp"` |
| `LongPressDialog` | home.cpp 已 include (line 15) |
| `ProgressDialog` | home.cpp 已 include (line 14) |
| `util::async` | home.cpp 已使用 |
| `fs::deleteFile` | 已有（fsHelper.hpp line 169） |
| `JsonFile::removeKey` | 已有 |

---

## 验证要点

1. 空游戏列表时也可点击（不设 disabled），流程正常走完弹完成提示
2. 进度条从 0 到 100% 正常递增（total=0 时跳过进度更新，防除零）
3. leftText 显示 displayName（用户自定义名优先，回滚链已在构造时处理）
4. 完成后 hasInstalledMod / modsDisabled 内存同步正确
5. downloadRecord.txt 删除后，下次进商店页面 load() 返空，标记消失 
6. Home 键在重置期间被禁用，完成后恢复
7. Home 是根 Activity 不会被 pop，无需 alive 保护
