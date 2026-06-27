# 商店游戏图标本地缓存实现计划

## 目标

为商店游戏列表增加本地永久 WebP 缓存，加载优先级为：

1. 当前列表项 `iconId`
2. 页面内 `TID -> textureId / -1` 纹理缓存
3. 本地 WebP 文件缓存
4. 网络图标请求

本地缓存通过 `ETag / Last-Modified` 自动更新。当前页面已经创建的纹理不热替换；如果网络条件请求发现服务端图标变化，只覆盖本地 WebP 和元数据，下次重新创建纹理时使用新图。

本计划只写实现方案，不要求当前阶段编译。

## 当前事实

- `StoreGameList` 已经负责商店游戏列表页的图标加载调度、主线程回调、纹理创建和释放。
- `StoreGameManager` 已经保存页面生命周期内的 `TID -> textureId / -1` 图标缓存，`reset()` 不清这个缓存。
- 当前 `submitNextIcons()` 会先看 `iconId`，再查页面内图标缓存，最后提交网络下载。
- 当前图标 worker 调用 `api::game::fetchIcon()`，完成后通过 `brls::sync` 回主线程。
- 当前 WebP decode、`nvgCreateImageRGBA()`、`setIcon()` 都在主线程路径。
- `ThreadPool` 是 4 个 worker，`submit` 是 fire-and-forget，任务必须自行检查 `stop_token`。
- `brls::sync` 只是排队到主线程执行，不会自动判断页面是否还活着。
- `api::game::fetchIcon()` 目前保持镜像优先、源站回退。
- `http::downloadImage(url, token)` 目前不支持自定义请求头，也不保存响应头。
- `fsHelper` 和 `JsonFile` 足够支撑本地 WebP 文件读写和 ETag 元数据保存。
- Sphaira appstore 的参考做法是：主线程按帧节流读取本地图标并显示，网络下载线程负责 `ETag / Last-Modified` 条件请求。

## 核心方案

采用接近 Sphaira 的分工：

- **主线程**负责本地图标链路。
- **线程池**负责网络图标请求。

主线程每帧最多尝试读取 2 个本地 WebP，避免一次性大量本地 IO 阻塞 UI。

线程池不单独拆“下载队列”和“校验队列”。校验就是带 `If-None-Match / If-Modified-Since` 的图标下载请求。

图标网络任务最多同时 3 个，普通下载和条件下载共用这个上限。线程池总共 4 个 worker，因此至少保留 1 个 worker 给分页请求或其他任务。

## 线程分工

### 主线程

负责：

- 扫描当前可见卡片。
- 检查当前列表项 `iconId`。
- 查询页面内 `TID -> textureId / -1` 缓存。
- 每帧最多读取 2 个本地 WebP。
- decode WebP。
- 创建 NVG 纹理。
- 更新当前列表项 `iconId`。
- 写页面内纹理缓存。
- 更新可见 cell 图标。
- 提交网络图标任务。

### 线程池

负责：

- 普通图标下载。
- 带 ETag 的条件下载。
- 保存网络返回的 WebP 到本地。
- 更新本地图标元数据。
- 请求完成后通过 `brls::sync` 回主线程。
- 普通下载和条件下载共用图标网络任务计数，最多同时 3 个。

线程池 worker 不允许：

- 访问 UI。
- 访问 `m_manager`。
- 创建或释放 NVG 纹理。
- 访问已析构页面对象。

## 主线程每帧限制

使用 Borealis 的每帧事件：

```text
brls::Application::getRunLoopEvent()
```

`StoreGameList` 在页面内容可用后订阅该事件，在析构时取消订阅。

每帧回调中：

1. 如果页面处于 placeholder 或 loading 状态，跳过。
2. 获取 `RecyclingGrid::getGridItems()` 当前可见 cell。
3. 按离当前焦点距离排序。
4. 本帧本地读取额度设为 2。
5. 只对需要图标的可见项尝试本地 WebP 读取。
6. 每尝试读取 1 个本地 WebP，消耗 1 个额度。
7. 额度用完后停止，下一帧重新获得 2 个额度。

这样实现的是：

```text
每帧最多 2 次主线程本地 WebP 读取
```

不是每轮、不是线程、不是队列长度。

## 图标填充流程

### 1. 当前列表项状态

主线程先看当前列表项 `iconId`：

- `> 0`：当前列表项已经有纹理，跳过。
- `-1`：失败过，跳过。
- `-2`：网络任务进行中，跳过。
- `0`：继续处理。

### 2. 页面内纹理缓存

如果 `iconId == 0`，主线程查询 `StoreGameManager` 的页面内缓存：

```text
TID -> textureId / -1
```

结果：

- `> 0`：直接回填当前列表项，更新可见 cell。
- `-1`：回填失败状态，跳过。
- `0`：页面缓存未命中，继续查本地 WebP。

### 3. 本地 WebP

页面缓存未命中时，主线程尝试读取本地 WebP。

本地读取路径：

```text
/config/NX-Mod-Manager/modShop/gameIcons/<tid>.webp
```

每帧最多尝试 2 个本地图标。

#### 本地读到

主线程：

1. decode WebP。
2. 创建 NVG 纹理。
3. 更新当前列表项 `iconId`。
4. 写页面内缓存：`TID -> textureId`。
5. 更新可见 cell。
6. 提交一个带 `ETag / Last-Modified` 的网络图标任务。

网络结果：

- `304`：本地文件未变化，结束。
- `200`：覆盖本地 WebP 和元数据，不热替换当前页面纹理。
- 失败：不改当前页面纹理，不永久缓存失败。

#### 本地没读到

主线程提交一个不带 `ETag / Last-Modified` 的网络图标任务。

提交前将当前列表项 `iconId` 标记为 `-2`，表示网络任务进行中。

网络结果：

- `200`：保存本地 WebP 和元数据，然后回主线程创建纹理并显示。
- 失败：回主线程写 `iconId = -1`，并写页面内缓存：`TID -> -1`。

## 网络图标任务

网络图标任务只有一种。

区别只在请求头：

- 没有条件请求头：普通下载。
- 带 `If-None-Match / If-Modified-Since`：条件下载，也就是校验。

网络任务继续保持：

- 镜像优先。
- 镜像失败且 token 未取消时回源站。
- `304` 单独作为“未变化”处理。
- `200 + body` 作为新图数据处理。

图标网络任务并发：

- 普通下载和条件下载共用同一个计数。
- 最多同时 3 个图标网络任务。
- 线程池总共 4 个 worker，至少留 1 个 worker 给分页请求或其他任务。

不新增独立校验线程，不新增独立校验队列。

## 本地缓存模块

新增 `StoreGameIconCache`，建议文件：

- `code/include/core/storeGameIconCache.hpp`
- `code/src/core/storeGameIconCache.cpp`

职责：

- 拼接本地 WebP 路径。
- 读取本地 WebP。
- 写入本地 WebP，先写临时文件，再替换目标文件。
- 加载和保存 `TID -> ETag / Last-Modified` 元数据。
- 运行中从内存查元数据。

默认路径：

```text
WebP: /config/NX-Mod-Manager/modShop/gameIcons/<tid>.webp
JSON: /config/NX-Mod-Manager/modShop/gameIconCache.json
```

元数据要求：

- JSON 启动或首次使用时读一次。
- 运行中查内存。
- 页面析构或缓存对象析构时保存。
- 只保存成功网络响应里的 `ETag / Last-Modified`。
- 本地失败不永久缓存。

## HTTP 和 API 扩展

### http

保留旧接口：

```text
downloadImage(url, token)
```

新增支持请求头和响应头的接口。

`Response` 增加响应头 map。

`Response::ok()` 不改，`304` 由图标逻辑单独判断。

### api::game

保留旧接口：

```text
fetchIcon(tid, token)
```

新增图标条件请求封装。

新封装返回：

- HTTP 状态码。
- WebP bytes。
- 响应头。

仍然保持镜像优先、源站回退。

## 生命周期要求

### 页面析构

析构顺序：

1. `m_stopSource.request_stop()`
2. 取消 `runLoopEvent` 订阅
3. 保存本地图标元数据
4. 遍历页面内纹理缓存，释放 `textureId > 0` 的 NVG 纹理

### reloadData

`reloadData()` 应：

- `request_stop()` 取消旧网络任务。
- 重建 `m_stopSource`。
- 重置当前列表和分页状态。
- 不清页面内 `TID -> textureId / -1` 缓存。
- 不删除本地 WebP 文件。
- 不释放仍在页面纹理缓存里的 NVG 纹理。

### brls::sync

所有网络任务回主线程的回调必须第一句检查 token：

```text
if (token.stop_requested()) return;
```

之后才允许访问：

- `this`
- UI
- NVG
- `m_manager`

## 分阶段实现

### 阶段 1：HTTP headers 能力

涉及文件：

- `code/include/utils/http.hpp`
- `code/src/utils/http.cpp`

目标：

- `Response` 增加响应头。
- 新增带请求 headers 的图片下载接口。
- 旧 `downloadImage(url, token)` 行为不变。

验收：

- 新接口能发送 `If-None-Match / If-Modified-Since`。
- 新接口能读取 `etag / last-modified`。
- `Response::ok()` 不改变。

### 阶段 2：api::game 条件请求

涉及文件：

- `code/include/api/game.hpp`
- `code/src/api/game.cpp`

目标：

- 保留旧 `fetchIcon()`。
- 新增带 headers 的图标请求封装。
- 镜像优先、源站回退不变。

验收：

- `304` 能被返回给调用方。
- `200` 能返回 WebP bytes 和响应头。

### 阶段 3：本地缓存模块

涉及文件：

- `code/include/core/storeGameIconCache.hpp`
- `code/src/core/storeGameIconCache.cpp`

目标：

- 实现本地 WebP 读写。
- 实现临时文件替换。
- 实现 ETag 元数据 load/save。

验收：

- 本地文件不存在时返回未命中。
- 写文件失败不破坏旧文件。
- JSON 不做每张图读写一次。

### 阶段 4：主线程每帧本地读取

涉及文件：

- `code/include/activity/storeGameList.hpp`
- `code/src/activity/storeGameList.cpp`

目标：

- 订阅 `runLoopEvent`。
- 每帧最多尝试读取 2 个本地 WebP。
- 只处理当前可见 cell。
- 页面纹理缓存命中时不读本地。

验收：

- 当前页面可见图标逐帧填充。
- 主线程不会一次性读取大量本地图标。
- 页面退出后不会继续执行每帧回调。

### 阶段 5：网络图标任务接入

涉及文件：

- `code/include/activity/storeGameList.hpp`
- `code/src/activity/storeGameList.cpp`

目标：

- 本地命中后提交带 ETag 的网络任务。
- 本地未命中后提交普通网络任务。
- 普通下载和条件下载共用并发上限，最多同时 3 个图标网络任务。
- 网络任务完成后按 token 安全回主线程。

验收：

- 本地命中先显示，`304` 不更新本地文件。
- 本地命中后 `200` 覆盖本地文件，但不热替换当前纹理。
- 本地未命中后 `200` 保存本地文件并显示。
- 网络失败写页面缓存 `TID -> -1`。
- 图标网络任务不会占满全部 4 个 worker，至少留 1 个 worker 给分页或其他任务。

### 阶段 6：生命周期和静态检查

目标：

- 检查 runLoopEvent 订阅和取消订阅。
- 检查所有 worker 捕获。
- 检查所有 `brls::sync` 第一句 token。
- 检查析构纹理释放。

验收：

- worker 不访问 UI。
- worker 不访问 `m_manager`。
- worker 不创建或释放 NVG 纹理。
- 页面析构后旧回调不访问页面。

## 明确不做

- 不热替换当前页面已经创建的纹理。
- 不做用户手动清理缓存入口。
- 不永久缓存失败状态。
- 不新增独立校验线程。
- 不新增独立校验队列。
- 不改变旧 `downloadImage(url, token)` 语义。
- 不改变旧 `fetchIcon(tid, token)` 语义。

## 静态检查清单

不编译程序。

建议检查：

```sh
rg "downloadImage|ETag|Last-Modified|If-None-Match|If-Modified-Since"
rg "getRunLoopEvent|runLoopEvent|unsubscribe|submitNextIcons|iconId"
rg "ThreadPool::instance\\(\\)\\.submit|brls::sync|stop_requested|request_stop"
rg "nvgCreateImageRGBA|nvgDeleteImage|iconCache|getCachedIcon|cacheIcon"
git diff --check
```

人工确认：

- 每帧本地 WebP 读取最多 2 个。
- 页面纹理缓存优先级最高。
- 本地命中后先显示，再条件下载。
- 本地未命中后普通下载。
- 条件下载返回 `304` 不改本地文件。
- 条件下载返回 `200` 只覆盖本地文件，不热替换当前纹理。
- 页面退出后 runLoopEvent 不再访问页面。
- 网络回调第一句检查 token。
