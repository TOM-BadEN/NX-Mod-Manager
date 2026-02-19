# nxtc 缓存版本过期修复方案

## 问题

`gameNACP.cpp` 读取 nxtc 缓存时，不比对游戏版本号。用户更新游戏后，缓存里的版本号、游戏名、图标仍是旧数据，永远不会刷新。

## 原因

1. 读缓存时只检查 `if (cached)`，不比对 `cached->version_info` 与系统实时版本
2. 写缓存时 `nxtcAddEntry(..., 0)` 传了 `0` 作为版本号，没有存入实时版本

## 参考项目的做法

- 用 `avmGetVersionListEntry(appId)` 获取系统实时版本号（`u32`）
- 读缓存时：`if (cached && live_version == cached->version_info)` 才命中
- 写缓存时：`nxtcAddEntry(..., live_version)` 存入实时版本

## 修复方案

### 涉及文件

- `gameNACP.cpp`（主要改动）
- `gameNACP.hpp`（如需新增私有方法）

### 具体步骤

**步骤1：新增 AVM 服务初始化/退出**

在 `GameNACP` 构造函数中加 `avmInitialize()`，析构函数中加 `avmExit()`。
AVM 服务提供 `avmGetVersionListEntry`，需要先初始化。

```cpp
GameNACP::GameNACP() {
    nsInitialize();
    avmInitialize();
    nxtcInitialize();
}

GameNACP::~GameNACP() {
    nxtcFlushCacheFile();
    nxtcExit();
    avmExit();
    nsExit();
}
```

**步骤2：新增获取实时版本的静态函数**

```cpp
static uint32_t getGameVersion(uint64_t appId) {
    AvmVersionListEntry entry;
    Result rc = avmGetVersionListEntry(appId, &entry);
    return R_SUCCEEDED(rc) ? entry.version : 0xFFFFFFFE;
}
```

返回 `0xFFFFFFFE` 作为失败标记（和参考项目一致），确保不会意外匹配到缓存中的任何真实版本。

**步骤3：修改 getGameNACP 的缓存读取逻辑**

```cpp
GameMetadata GameNACP::getGameNACP(uint64_t appId) {
    GameMetadata meta;
    uint32_t liveVersion = getGameVersion(appId);

    // 查缓存 + 比对版本
    NxTitleCacheApplicationMetadata* cached = nxtcGetApplicationMetadataEntryById(appId);
    if (cached && liveVersion == cached->version_info) {
        // 版本匹配，用缓存
        ...
        return meta;
    }
    if (cached) nxtcFreeApplicationMetadata(&cached);

    // 缓存未命中或版本不匹配，走 NS 服务
    ...
}
```

关键变化：`if (cached)` → `if (cached && liveVersion == cached->version_info)`

**步骤4：修改写缓存时传入实时版本**

```cpp
nxtcAddEntry(appId, &controlData->nacp, iconSize, controlData->icon, false, liveVersion);
```

`0` → `liveVersion`

### 注意事项

- `avmInitialize()` 可能失败（某些自制系统固件），需要容错处理
- 失败时 `getGameVersion` 返回 `0xFFFFFFFE`，永远不匹配缓存 → 每次都走 NS 服务（退化为无缓存模式，功能不受影响，只是慢一点）
- 改动只在 `gameNACP.cpp` 内部，对外接口 `getGameNACP(appId)` 不变，`home.cpp` 无需改动