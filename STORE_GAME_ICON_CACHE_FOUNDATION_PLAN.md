# 商店游戏图标缓存底层支持计划

## 目标

先实现通用、解耦的底层能力，再接入 `StoreGameList` 页面。

底层支持只提供能力，不绑定商店游戏列表页面生命周期，不创建 NVG 纹理，不操作 UI，不读写 `StoreGameManager`。

后续页面功能依赖这些底层能力实现：

- 本地 WebP 图标缓存。
- `ETag / Last-Modified` 条件请求。
- 镜像优先、源站回退。
- 本地元数据保存。

## 设计原则

- HTTP 层只关心请求和响应，不知道游戏图标。
- API 层只关心业务 URL 和镜像回退，不知道页面 UI。
- 本地缓存层只关心文件和元数据，不知道 NVG 纹理。
- 页面层才负责主线程读取节流、纹理创建、UI 更新、页面内 `TID -> textureId / -1` 缓存。

## 底层支持一：downloadImage 可选 headers 重载

### 涉及文件

- `code/include/utils/http.hpp`
- `code/src/utils/http.cpp`

### 目标

在不改变现有 `downloadImage(url, token)` 语义的前提下，为图片下载增加一个可选 headers 重载。

这个能力只扩展图片下载入口，不改整个 HTTP 工具的调用方式。

不是专门为图标写死字段，而是让需要条件请求的调用方可以：

- 传入额外请求头。
- 读取响应头。

### 建议接口

保留旧接口：

```text
downloadImage(url, token)
```

新增 headers 类型：

```text
http::Headers
```

新增 `downloadImage` 重载：

```text
downloadImage(url, headers, token)
```

`Response` 增加：

```text
headers
```

### 关键要求

- 旧 `downloadImage(url, token)` 行为不变。
- `Response::ok()` 不改变。
- `304` 不应该被 `ok()` 视为成功，由上层封装按业务判断。
- 响应头 key 建议统一转小写，便于读取 `etag`、`last-modified`。
- 请求头不只服务 ETag，应该是通用 map。
- HTTP 层不写任何商店图标路径。
- HTTP 层不关心 `TID`。
- 页面层不直接使用这个 HTTP Response 处理图标业务，应通过 `api::game` 封装。

### 验收点

- 旧调用点无需修改。
- 新接口可以发送 `If-None-Match`。
- 新接口可以发送 `If-Modified-Since`。
- 新接口可以读取响应 `etag`。
- 新接口可以读取响应 `last-modified`。

## 底层支持二：api::game 图标条件请求

### 涉及文件

- `code/include/api/game.hpp`
- `code/src/api/game.cpp`

### 目标

在业务 API 层封装图标网络请求。

页面不直接拼图标 URL，不直接处理镜像和源站。

### 建议接口

保留旧接口：

```text
fetchIcon(tid, token)
```

新增一个支持条件请求的图标接口，命名待实现时确定，例如：

```text
fetchIconConditional(tid, validator, token)
```

其中 `validator` 是一个业务结构体，不是 HTTP headers：

```text
IconCacheValidator
- hasLocalFile
- etag
- lastModified
```

返回结果不直接暴露 `http::Response`，而是返回清晰的业务字段：

- success
- error
- notModified
- hasData
- WebP bytes
- etag
- lastModified

### 关键要求

- 镜像优先。
- 镜像失败后，token 未取消才回源站。
- `validator.hasLocalFile == true` 时，API 内部根据 `etag / lastModified` 生成条件请求头。
- `validator.hasLocalFile == false` 时，即使 `etag / lastModified` 有值，也不发送条件请求头。
- `304` 转成 `notModified = true`。
- `200 + body` 转成 `success = true`、`hasData = true` 和 WebP bytes。
- 成功响应里的 `etag / last-modified` 转成结果字段。
- 旧 `fetchIcon()` 行为不变。
- API 层不操作本地文件。
- API 层不创建纹理。
- API 层不操作 UI。
- API 层内部可以使用 HTTP code 和 headers，但不把原始 `Response` 交给页面层。
- 页面层不直接拼 `If-None-Match / If-Modified-Since`。

### 验收点

- 普通下载可用。
- 条件请求可用。
- 镜像和源站都走同一套条件请求参数。
- `304` 不被当成网络失败丢掉，而是表现为 `notModified = true`。
- 页面层不需要判断 HTTP code。
- 页面层不需要构造 HTTP headers。

## 底层支持三：本地文件缓存模块

### 建议新增文件

- `code/include/core/storeGameIconCache.hpp`
- `code/src/core/storeGameIconCache.cpp`

### 目标

封装商店游戏图标本地 WebP 文件和 HTTP 缓存元数据。

这个模块虽然服务商店游戏图标，但仍然不依赖页面，不依赖 NVG，不依赖 UI。

这个模块按当前项目线程模型设计为主线程调用，不让 worker 线程直接访问。

### 默认路径

```text
WebP: /config/NX-Mod-Manager/modShop/gameIcons/<tid>.webp
JSON: /config/NX-Mod-Manager/modShop/gameIconCache.json
```

### 建议能力

- 根据 TID 生成 WebP 路径。
- 判断本地 WebP 是否存在。
- 读取本地 WebP bytes。
- 写入本地 WebP bytes。
- 写入时先写临时文件，再替换目标文件。
- 删除临时文件残留。
- 读取某个 TID 的 `ETag / Last-Modified` 元数据。
- 生成某个 TID 的缓存验证标识：
  - `hasLocalFile`
  - `etag`
  - `lastModified`
- 根据 `api::game` 返回的 `etag / lastModified` 更新某个 TID 的元数据。
- 加载元数据 JSON。
- 保存元数据 JSON。

### 元数据要求

- JSON 启动或首次使用时读一次。
- 运行中查内存。
- 运行中更新内存状态。
- `StoreGameList` 析构时主动调用保存。
- `StoreGameIconCache` 析构不自动保存，避免重复保存和隐藏副作用。
- 只保存成功网络响应里的 `ETag / Last-Modified`。
- 不永久缓存失败状态。
- JSON 不做每张图读写一次。

### 线程要求

- `StoreGameIconCache` 默认不跨线程共享。
- 页面主线程负责调用本地 WebP 读写和元数据读写。
- worker 线程不捕获、不访问 `StoreGameIconCache`。
- worker 线程只拿 `tid / IconCacheValidator` 等值拷贝数据执行网络请求。
- 网络任务回到 `brls::sync` 后，第一句检查 token，未取消才允许主线程写本地 WebP 和更新元数据。
- 读写 WebP 文件本身不触碰页面 UI 对象。
- 模块本身不构造 `If-None-Match / If-Modified-Since`。
- 模块本身不持有 `StoreGameList`。
- 模块本身不访问 `StoreGameManager`。

### 验收点

- 本地文件不存在时返回未命中。
- 本地文件存在时能读出 bytes。
- 写文件失败不破坏旧文件。
- 临时文件失败能清理。
- 元数据 JSON 缺失时能从空表开始。
- 元数据可读可写。
- 本地 WebP 文件不存在时，生成的验证标识必须是 `hasLocalFile = false`。

## 底层支持四：缓存验证标识构建约定

### 目标

避免页面直接拼 HTTP 头字段，也避免本地缓存模块理解 HTTP 请求细节。

本地缓存模块只提供缓存验证标识，`api::game` 根据这个标识在内部决定是否构造条件请求头。

### 建议能力

输入：

```text
tid
```

输出：

```text
IconCacheValidator
- hasLocalFile
- etag
- lastModified
```

规则：

- 只有本地 WebP 存在时，`hasLocalFile` 才能为 `true`。
- 如果本地 WebP 不存在，即使 JSON 里残留 ETag，也必须返回 `hasLocalFile = false`。
- `StoreGameIconCache` 可以返回 `etag / lastModified` 字段，但不负责把它们变成 HTTP headers。
- `api::game` 只有在 `hasLocalFile = true` 时，才根据 `etag / lastModified` 构造条件请求头。

原因：

如果本地文件不存在却发送旧 ETag，服务器可能返回 `304`，此时本地没有可用图标。

### 验收点

- 本地有文件且有 ETag：验证标识里 `hasLocalFile = true`，并带出 `etag`。
- 本地有文件且有 Last-Modified：验证标识里 `hasLocalFile = true`，并带出 `lastModified`。
- 本地无文件：验证标识里 `hasLocalFile = false`。
- 页面层不构造 `If-None-Match / If-Modified-Since`。
- 本地缓存层不构造 `If-None-Match / If-Modified-Since`。
- 只有 `api::game` 内部构造 `If-None-Match / If-Modified-Since`。

## 底层支持五：结果结构约定

### 目标

让页面层拿到清晰结果，不在页面里猜 HTTP 含义。

### 建议图标网络结果

需要能表达：

- 网络失败。
- 未变化。
- 拿到新图数据。
- 新的 ETag。
- 新的 Last-Modified。

建议字段：

```text
success
error
notModified
hasData
data
etag
lastModified
```

页面层只需要判断：

- `notModified`：本地文件未变化。
- `hasData`：拿到新 WebP。
- `success == false`：网络失败。

### 建议本地读取结果

需要能表达：

- 未命中。
- 读到 bytes。
- 文件损坏或读取失败。

文件损坏是否特殊处理，可以后续页面接入时再定。当前策略可以延续原行为，不额外复杂化。

## 不属于底层支持的内容

以下内容不要在底层模块里做：

- 订阅 `runLoopEvent`。
- 每帧最多读取 2 个图标。
- 扫描当前可见 cell。
- 创建 NVG 纹理。
- `setIcon()`。
- 写 `GameList.iconId`。
- 写页面内 `TID -> textureId / -1` 缓存。
- 释放 NVG 纹理。
- 页面 `request_stop()` 生命周期控制。

这些属于 `StoreGameList` 页面接入阶段。

## 推荐实现顺序

### 阶段 1：downloadImage headers 重载

先完成 `downloadImage` 的可选 headers 重载。

验收：

- 旧接口不变。
- 新重载能发送请求头。
- 新重载能读取响应头。

### 阶段 2：api::game 条件请求

基于 `downloadImage` headers 重载，完成图标条件请求封装。

验收：

- 普通下载可用。
- 条件下载可用。
- `304` 转成业务字段 `notModified`。
- `200 + body` 转成业务字段 `hasData` 和 WebP bytes。
- 镜像优先、源站回退不变。

### 阶段 3：StoreGameIconCache

完成本地 WebP 和元数据模块。

验收：

- 本地 WebP 读写可用。
- ETag 元数据 load/save 可用。
- 可以生成 `IconCacheValidator`。
- 本地无文件时生成 `hasLocalFile = false`，不让后续 API 发条件请求头。
- 模块设计为主线程调用，不要求 worker 线程直接访问。

### 阶段 4：底层静态检查

不接页面前先检查：

- HTTP 层不含商店页面逻辑。
- API 层不含文件 IO。
- 本地缓存层不含 UI/NVG。
- 本地缓存层不被 worker 线程直接捕获使用。
- 旧接口语义不变。

## 静态检查清单

不编译程序。

建议命令：

```sh
rg "downloadImage|Headers|etag|last-modified|If-None-Match|If-Modified-Since" code/include/utils code/src/utils
rg "fetchIcon|IconCacheValidator|iconMirror|icon\\(" code/include/api code/src/api
rg "StoreGameIconCache|IconCacheValidator|gameIconCache|gameIcons"
rg "nvgCreateImageRGBA|setIcon|StoreGameList|StoreGameManager" code/include/core code/src/core code/include/api code/src/api code/include/utils code/src/utils
git diff --check
```

人工确认：

- 底层模块不创建纹理。
- 底层模块不操作 UI。
- 底层模块不访问 `StoreGameList`。
- 底层模块不访问 `StoreGameManager`。
- worker 线程不捕获 `StoreGameIconCache`。
- 旧 HTTP 和旧 `fetchIcon()` 调用不需要改。
