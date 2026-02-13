# 扫描逻辑设计计划书

## 参考项目：每个信息的获取方式

### 目录结构

```
/mods2/
├── game_name.json                    ← 游戏显示名映射 + 收藏分组
├── 游戏名A[1.0.2]/                   ← 目录名包含版本号
│   └── 0100A5C00D162000/             ← appId（16进制）
│       ├── mod1/                     ← 一个 mod
│       ├── mod2.zip                  ← 也算一个 mod
│       └── mod_name.json            ← mod 显示名映射
├── 游戏名B/                          ← 没有版本号
│   └── 01001A5C00E3A000/
│       └── ...
```

### 1. name（显示名称）

- 启动时先读 `/mods2/game_name.json`，加载映射缓存
- JSON 格式：
  ```json
  {
    "游戏名A[1.0.2]": "自定义显示名A",
    "游戏名B": "自定义显示名B",
    "favorites$180": {
      "游戏名C[1.0.0]": "自定义显示名C"
    }
  }
  ```
- 扫描时用目录名去缓存里查：查到用映射名，查不到用目录名本身

### 2. version（版本号）

- 直接从目录名解析，格式 `游戏名[1.0.2]`，提取方括号内的内容
- 没有方括号则标记为无版本

### 3. modCount（mod 数量）

- 进入 `/mods2/游戏名/appId/` 目录
- 数里面的子目录数 + zip 文件数 = mod 总数

### 4. appId（游戏唯一 ID）

- `/mods2/游戏名/` 下有一个 16 进制命名的子目录（如 `0100A5C00D162000`）
- 把目录名用 strtoull 转成 u64 就是 appId

### 5. icon（游戏图标）

- 拿到 appId 后，两级回退：
  1. 先查本地文件缓存（libnxtc 库）
  2. 缓存没有 → 调 Switch 系统 API `nsGetApplicationControlData()` 获取 JPEG 数据
- 拿到 JPEG 后用 `nvgCreateImageMem()` 创建 NVG 图像 ID

### 扫描顺序

1. 读 `game_name.json` → 得到收藏列表 + 非收藏列表 + 显示名映射
2. 扫描 `/mods2/` 目录 → 得到所有游戏目录名
3. 去重：去掉 JSON 中已有的目录名，剩下的是新目录
4. 合并排序：收藏 → 非收藏 → 新目录
5. 遍历合并后的列表，逐个获取 appId、modCount、icon

### 参考项目的数据结构体（主页用）

```cpp
struct AppEntry final {
    std::string name;              // 系统 API 拿到的游戏名（原始名）
    std::string display_version;   // 系统 API 拿到的版本号
    AppID id;                      // appId（u64）
    int image;                     // NVG 图像 ID（图标）
    bool selected{false};          // 选中状态
    bool own_image{false};         // 图标是否自己创建的（用于释放资源）

    std::vector<unsigned char> cached_icon_data;  // 缓存的 JPEG 原始数据
    bool has_cached_icon{false};                  // 是否有缓存图标

    std::string FILE_NAME;         // 目录名（方括号前的部分）
    std::string FILE_NAME2;        // 最终显示名（JSON 映射名，没有则用 FILE_NAME）
    std::string FILE_PATH;         // 完整路径 /mods2/游戏名/appId
    std::string MOD_VERSION;       // 目录名方括号里的版本号
    std::string MOD_TOTAL;         // mod 数量（字符串）
    size_t unique_id;              // 唯一标识（用于异步图标加载匹配）
    bool is_favorite{false};       // 是否收藏
};
```

---

## 我们的项目：设计方案

### 数据结构体

```cpp
struct GameInfo {
    std::string displayName;  // 显示名（回滚链：displayName → gameName → 目录名）
    std::string gameName;    // API 名缓存（从 JSON 读，第二阶段 API 更新）
    std::string version;     // 版本号（从 JSON 缓存读，第二阶段 API 更新）
    std::string modCount;    // mod 数量
    int iconId = 0;          // NVG 图标 ID
    u64 appId = 0;           // 游戏唯一 ID
    std::string dirPath;     // 完整路径 /mods2/dirName/appIdHex
    bool isFavorite = false; // 是否收藏
};
```

### JSON 文件：`/mods2/gameInfo.json`

用 appId 做根键，只存用户自定义数据：

```json
{
  "0100A5C00D162000": {
    "displayName": "塞尔达传说",
    "gameName": "ゼルダの伝説 ティアーズ オブ ザ キングダム",
    "favorite": true,
    "version": "1.2.1"
  },
  "01001A5C00E3A000": {
    "gameName": "Super Mario Odyssey",
    "favorite": false,
    "version": "1.0.0"
  }
}
```

- **根键**：appId（16 位十六进制），永远不变，不受目录重命名影响
- **displayName**：用户手动设置的自定义名，没设过则不存这个字段
- **gameName**：API 名缓存，第二阶段始终从 API 更新
- **favorite**：收藏标记，true/false 直接切换
- **version**：缓存字段，第一阶段快速显示，第二阶段从 API 获取最新值并更新
- **显示名回滚链**：displayName → gameName → 目录名
- **不存游戏列表**：游戏列表每次启动重新扫描 `/mods2/` 目录

### 扫描策略：两阶段扫描

#### 第一阶段：快速扫描（只读目录 + JSON，不调 API）

1. 读 `/mods2/gameInfo.json` → 加载 displayName、gameName、version、isFavorite 映射
2. 扫描 `/mods2/` → 拿到所有游戏目录名
3. 进入每个目录，读 hex 子目录名 → 拿到 appId
4. 数 appId 目录下的子目录 + zip → 拿到 modCount
5. 用 appId 查 JSON → 拿到 displayName、gameName、version、isFavorite
6. 确定显示名：displayName → gameName → 目录名
7. 拼 dirPath = `/mods2/目录名/appIdHex`
8. 排序：收藏在前，按显示名拼音排序
9. 一次性传给 GridPage 显示（版本从 JSON 缓存读，图标用默认图标）

#### 第二阶段：逐个调 API（异步，边扫边更新）

按排好的顺序，逐个用 appId 调 API（先缓存库，再系统 API）：
- 拿到 version → 更新对应卡片
- 拿到 icon → 更新对应卡片

#### 用户体验

- 打开软件 → 几乎瞬间看到完整列表（有名字、有 mod 数量、排好序）
- 然后图标和版本号逐个"冒出来"

### 安全线程机制

所有后台任务（扫描、安装、卸载、下载等）统一使用此机制。

#### 核心：AsyncFurture + brls::sync

直接复用参考项目的 `async.hpp`（放到 `code/include/utils/async.hpp`）。

- **后台线程**：用 `util::async()`，基于 `std::async`，每个任务独立线程
- **停止信号**：C++20 `std::stop_token`，标准方案
- **UI 更新**：用框架的 `brls::sync()`，投递到主线程执行
- **不用 `brls::async`**：它是单线程顺序执行，多任务会互相阻塞

#### AsyncFurture（来自参考项目）

封装 `std::future` + `std::stop_source`：
- 析构时自动 `request_stop()` + `get()`（等线程结束）
- 手动取消：`request_stop()` + `get()`

#### 使用方式

```cpp
// 成员变量
util::AsyncFurture<void> m_task;

// 启动
m_task = util::async([this](std::stop_token token) {
    for (...) {
        if (token.stop_requested()) break;
        // 干活...
        auto stopToken = token;
        brls::sync([stopToken]() {
            if (stopToken.stop_requested()) return;
            // 更新 UI
        });
    }
});

// 手动取消
m_task.request_stop();
m_task.get();

// 析构时自动取消，不用手写
```

#### 为什么安全

- 析构自动 `request_stop` + `get`，线程结束后对象才销毁
- `stop_token` 是值类型，sync 回调捕获拷贝后即使原对象已销毁也安全
- 退出 App 时，析构链调用各 `AsyncFurture` 析构，所有线程安全退出

#### 大操作的响应速度

- 小步骤（如逐个文件复制）：每步之间检查开关，几乎秒停
- 大操作（如复制大文件）：分块处理，每块检查一次

```cpp
while (bytesRemaining > 0) {
    if (token.stop_requested()) break;
    size_t chunk = std::min(bytesRemaining, CHUNK_SIZE); // 如 1MB
    fread(buf, 1, chunk, src);
    fwrite(buf, 1, chunk, dst);
    bytesRemaining -= chunk;
    // 顺便更新进度条
}
```

#### 适用场景

| 场景 | 谁来关开关 |
|---|---|
| 主页扫描图标 | 主页不会销毁，通常跑完 |
| 详情页扫描 mod → 按 B | 析构函数关 |
| 安装/卸载 → 用户取消 | 取消按钮关 |
| 下载 → 用户取消 | 取消按钮关 |

### 图标加载策略

#### 数据分离

| 数据 | 存储位置 | 何时创建 | 何时释放 |
|---|---|---|---|
| version | GameInfo.version | 第一阶段从 JSON 缓存读，第二阶段 API 更新 | 不释放 |
| JPEG 字节 | 独立缓存 map（appId → JPEG） | API 拿到就存 | 不释放（~50KB/个） |
| NVG 纹理 | GameInfo.iconId | 进入可见范围时从 JPEG 创建 | 离开可见范围时 nvgDeleteImage |

#### 可见范围

当前页 ± 1 页 = 最多 27 个 NVG 纹理

#### 后台线程：优先加载当前页附近

- 用 `std::atomic<int>` 记录当前页码
- 后台线程每次循环找离当前页最近的未加载游戏
- 用户翻页时更新页码，后台自动切换优先级
- 每加载完一个，brls::sync 回主线程：
  - 存 version 到 GameInfo
  - 存 JPEG 到缓存
  - 如果在可见范围内 → 创建 NVG → 更新卡片

#### 翻页时（主线程）

- 新可见范围内：已有 JPEG 但没 NVG → 从 JPEG 创建 NVG（~2-5ms/个，瞬间）
- 离开可见范围：有 NVG → nvgDeleteImage 释放，JPEG 保留
- 翻回之前看过的页 → JPEG 还在缓存，直接重建 NVG，不用重新调 API

#### 内存预估（500 个游戏）

- JPEG 缓存：500 × 50KB ≈ 25MB（RAM）
- NVG 纹理：27 × 300KB ≈ 8MB（GPU）

### 每个字段的获取方式

#### ~~1. dirName~~ → 已去掉，不需要单独保存

- 扫描时作为临时变量使用，不存入结构体
- 需要时可从 dirPath 截取

#### 2. appId（第一阶段）

- 进入 `/mods2/游戏目录名/`，遍历子目录
- 找到 hex 命名的子目录，用 `strtoull` 转成 u64
- 转换失败或为 0 则跳过这个游戏

#### 3. modCount（第一阶段）

- 跟 appId 同一次遍历，进入 `/mods2/游戏目录名/appIdHex/`
- 子目录 → 算一个 mod
- `.zip`/`.ZIP`/`.Zip` 等任意大小写的 zip 后缀文件 → 算一个 mod（忽略大小写）
- 其他文件不算

#### 4. dirPath（第一阶段）

- 拼接：`"/mods2/" + 目录名 + "/" + appIdHex`
- 前面的信息都有了，纯字符串拼接

#### 5. version（第一阶段 + 第二阶段）

- **第一阶段**：从 gameInfo.json 读缓存的 version，骨架时直接显示
  - JSON 没有（首次运行）→ 显示 `"..."`
- **第二阶段**：从 API 获取最新 version
  - 先查 libnxtc 缓存：`nxtcGetApplicationMetadataEntryById(appId)`
  - 缓存没有 → 调系统 API `nsGetApplicationControlData()`
  - 从 NacpStruct 读 `display_version`
  - 如果跟 JSON 缓存不同 → 更新 GameInfo + 更新卡片 + 写回 JSON
  - 跟 iconId 来自同一个 API 调用，一次拿两个

#### 6. iconId（第二阶段）

- 跟 version 同一个 API 调用拿到 JPEG 数据
- 用 `nvgCreateImageMem()` 转成 NVG 图像 ID
- 调 `GameCard::setIcon(iconId)` 更新卡片
- 第一阶段不需要处理，GameCard 默认就有占位图标

#### 7. displayName（第一阶段 + 第二阶段）

显示名回滚链：displayName → gameName → 目录名

- **第一阶段**：用 appId 查 gameInfo.json
  - 有 `displayName`（用户手动设的）→ 用它
  - 没有 `displayName`，有 `gameName`（API 名缓存）→ 用它
  - 都没有（首次运行）→ 用目录名
- **第二阶段**：从 API 获取官方游戏名
  - 始终更新 JSON 的 `gameName` 字段
  - 如果当前显示名是目录名（无 displayName 且无旧 gameName）→ 更新卡片显示为 gameName
  - 如果当前显示名是用户设的 displayName → 不更新卡片显示

#### 8. isFavorite（第一阶段）

- 跟 displayName 同一次 JSON 查询
- 查到 → 读 `favorite` 字段
- 查不到 → 默认 false
