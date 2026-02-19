# 异步体积计算方案

## 1. JsonFile 提升为 ModList 成员

- ModList 新增 `JsonFile m_modJson` 成员（参考主页 `m_jsonCache`）
- `onContentAvailable` 中 `m_modJson.load(tidPath + config::modInfoFile)`
- ModScanner 改为接收 `JsonFile&` 参数，不再自己创建局部 JsonFile
- 体积算完后通过 `m_modJson.setString()` + `m_modJson.save()` 写回

## 2. 后台线程 startSizeLoader()

- 使用 `std::jthread` + `stop_token`，页面退出时自动停止
- 构建任务列表：遍历 `m_mods`，`size` 为空的加入任务
- 初始排序：按离当前焦点（`m_focusedIndex`）的距离，近的优先
- **不打断**：正在计算的 mod 不中断，算完后选下一个任务时重新按焦点距离排序
- 逐个计算：`fs::calcDirSize(mod.path)` → 格式化为可读字符串

## 3. 主线程更新（brls::sync）

- 每个 mod 算完后 `brls::sync` 回主线程：
  - `m_mods[i].size = 格式化字符串`
  - 如果 `i == m_lastFocusIndex` → `m_tagSize->setText(...)`（更新详情面板）
  - `m_modJson.setString(dirName, "size", ...)`（写入 JSON 内存）
- 全部完成后 `brls::sync` → `m_modJson.save()`（统一写回文件）

## 4. 焦点追踪

- ModList 新增 `std::atomic<size_t> m_focusedIndex{0}`
- `focusChangeCallback` 中 `m_focusedIndex.store(index)`

## 5. fs::calcDirSize()

- 返回 `int64_t`（精确字节数），不做格式化
- 格式化由调用者负责（startSizeLoader 中格式化后再传给主线程）
- 具体实现细节待定