# 异步体积计算方案

## 1. JsonFile 提升为 ModList 成员

- ModList 新增 `JsonFile m_modJson` 成员（参考主页 `m_jsonCache`）
- `onContentAvailable` 中 `m_modJson.load(m_dirPath + config::modInfoFile)`
- ModScanner::scanMods 改为接收 `JsonFile&` 参数，不再自己创建局部 JsonFile
- 体积算完后通过 `m_modJson.setString()` 写入内存，全部完成后 `m_modJson.save()` 写回文件

## 2. 后台线程 startSizeLoader()

- 使用 `util::async`（AsyncFurture），参考主页 `startNacpLoader` 的模式
- 回调接收 `std::stop_token`，析构时自动 request_stop + 等待完成
- 构建任务列表：遍历 `m_mods`，`size` 为空的索引加入 `std::vector<int> tasks`
- **焦点优先**：每次选下一个任务时，找离 `m_focusedIndex.load()` 最近的
- **不打断**：正在 `calcDirSize` 的不中断，算完后才选下一个
- 逐个计算：`fs::calcDirSize(mod.path)` → `format::fileSize(bytes)` → 可读字符串
- 每个算完后 `brls::sync` 回主线程更新

## 3. 主线程更新（brls::sync）

- 每个 mod 算完后 `brls::sync` 回主线程：
  - `m_mods[i].size = 格式化字符串`
  - 如果 `i == m_lastFocusIndex` → `m_tagSize->setText(...)`（实时更新详情面板）
  - `m_modJson.setString(dirName, "size", 格式化字符串)`（写入 JSON 内存）
- 全部完成后 `brls::sync` → `m_modJson.save()`（统一写回文件）

## 4. 焦点追踪

- ModList 新增 `std::atomic<size_t> m_focusedIndex{0}`
- `focusChangeCallback` 中 `m_focusedIndex.store(index)`

## 5. fs::calcDirSize() ✅ 已完成

- 非递归迭代，堆上 vector 做目录栈
- 分批读 64 条/批，固定 49KB 栈空间
- 返回 `int64_t` 精确字节数，格式化由调用者负责

## 6. format::fileSize() ✅ 已完成

- `format.hpp` 中 inline 函数
- 按 B / KB / MB / GB 格式化

## 7. updateDetail 改动

- 当前硬编码 `"正在计算体积..."`
- 改为：`mod.size.empty() ? "正在计算体积..." : mod.size`

## 改动文件清单

| 文件 | 改动 |
|------|------|
| `modList.hpp` | 新增 `JsonFile m_modJson`、`util::AsyncFurture<void> m_sizeLoader`、`std::atomic<size_t> m_focusedIndex{0}`、`void startSizeLoader()` |
| `modList.cpp` | onContentAvailable 中 load modJson + 传给 scanner + 调 startSizeLoader；focusChangeCallback 更新 m_focusedIndex；实现 startSizeLoader；updateDetail 改 tagSize 逻辑 |
| `modScanner.hpp` | scanMods 签名改为 `scanMods(const std::string& tidPath, JsonFile& json)` |
| `modScanner.cpp` | 删除局部 JsonFile 创建和 load，改用传入的引用 |
| `fsHelper.hpp/cpp` | ✅ 已完成 calcDirSize |
| `format.hpp` | ✅ 已完成 fileSize |