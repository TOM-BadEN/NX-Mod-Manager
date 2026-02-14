# 第二阶段：异步加载 NACP 数据

## 触发时机

第一阶段结束后（`setGameList` 完成，九宫格已显示）

## 数据存储

| 数据 | 存储位置 | 说明 |
|---|---|---|
| version / displayName | `m_games[idx]`（GameInfo） | 直接更新结构体字段 |
| NVG 纹理 ID | `m_games[idx].iconId` | `0`=无纹理，`>0`=有效纹理，常驻不释放 |
| JSON 缓存 | `JsonFile` 对象 | 内存中，更新后写回文件 |
| 当前页码 | `atomic<int> currentPage` | 后台线程读，主线程写 |
| 待加载任务 | `vector<LoadTask>` | 后台线程内部，执行完就删除 |

## 图标策略（简化版）

- API 返回 JPEG → 立即 `nvgCreateImageMem` 转成 NVG 纹理 → JPEG 丢弃
- NVG 纹理常驻内存，不释放（参考项目也是这么做的）
- 翻页时不需要特殊处理图标，`refreshPage()` 现有逻辑已覆盖
- 不需要 JPEG 缓存 map，不需要 `loaded` 字段

## 后台线程（util::async）

```
启动前：从 m_games 提取 LoadTask 列表（idx + appId）

循环 {
    1. 检查 stop_token，被请求停止则 break
    2. tasks 为空 → 全部加载完 → break，线程自然结束

    3. 读取 atomic<int> currentPage
    4. 从 tasks 中找离 currentPage 最近的任务
    5. 从 tasks 中删除该任务（不会重复加载）

    6. 调 GameNACP::getGameNACP(appId)
       → 一次拿到 name + version + icon JPEG

    7. JPEG 非空 → nvgCreateImageMem(vg, ...) → 得到 iconId（后台线程执行）
       JPEG 用完即丢，不缓存

    8. brls::sync 回主线程 {
       a. 更新 version:
          - API 返回的 version 非空 → 更新 m_games[idx].version
          - 写回 JSON

       b. 更新 displayName:
          - JSON 里有 displayName → 不动（用户自定义优先）
          - JSON 里没有 displayName → 更新 m_games[idx].displayName 为 API 名
          - 始终写 gameName 到 JSON

       c. m_games[idx].iconId = iconId

       d. 调 gridPage->updateCard(idx) 刷新卡片

       e. 写回 JSON
    }
}
```

## 翻页时（主线程）

```
1. 更新 atomic<int> currentPage
2. refreshPage() 自动用 m_games 里的最新数据刷新卡片
   - version/displayName 用最新值
   - iconId > 0 则显示图标
   - 不需要额外处理
```

## 线程终止

- **正常完成**：tasks 为空，循环 break，线程自然结束
- **外部取消**：AsyncFurture 析构时自动 request_stop() + get()
- MainActivity 是主页面不会被 pop，实际上线程都是正常跑完的

## 动态优先级

后台线程不是从头到尾顺序加载，而是每次找离当前页最近的未加载游戏：

```
用户在第 5 页 → 加载顺序：
第5页(9个) → 第4页 → 第6页 → 第3页 → 第7页 → ...

用户翻页 → atomic<int> currentPage 更新
→ 后台线程下一轮循环自动切到新页附近
```

## 已完成的修改

1. **gameInfo.hpp** — 加 `bool loaded = false`（待移除，不再需要）
2. **main_activity.hpp** — `m_games` 改为成员变量（✅ 已完成）
3. **gridPage.hpp/cpp** — `m_games` 改为指针不拷贝（✅ 已完成）
4. **gridPage.hpp/cpp** — 加 `getCurrentPage()` + `updateCard(int globalIndex)`（✅ 已完成）

## 待实现

1. **gameInfo.hpp** — 移除 `bool loaded` 字段
2. **main_activity.hpp/cpp** — 加 `AsyncFurture` + `atomic<int>` + `JsonFile`，启动异步任务