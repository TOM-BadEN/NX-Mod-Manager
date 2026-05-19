/**
 * ModFileRefCount - MOD 共享文件引用计数
 * 当多个 MOD 安装了相同文件（CRC32 一致）时，记录共享次数
 * 
 * 计数语义：记录"有多少个额外 MOD 共享该文件"（不含首次写入者）
 *   - 首次写入文件 → 不记录（无 key）
 *   - 第二个 MOD 共享 → increment → 计数 1
 *   - 第三个 MOD 共享 → increment → 计数 2
 * 
 * 卸载判定：
 *   - 有记录 → decrement，不删文件（还有其他 MOD 在用）
 *   - 无记录 → 返回 true，调用方应删除文件（最后一个使用者）
 */

#pragma once

#include <string>
#include "utils/jsonFile.hpp"

class ModFileRefCount {
public:
    ModFileRefCount() = default;

    /**
     * @brief 加载指定游戏的引用计数文件，文件不存在时自动创建
     * @param path 引用计数文件路径
     */
    bool load(const std::string& path);

    /** @brief 持久化到文件 */
    bool save();

    /**
     * @brief 共享计数 +1（安装时目标文件已存在且 CRC 一致时调用）
     * @param filePath 目标文件路径
     */
    void increment(const std::string& filePath);

    /**
     * @brief 卸载时调用：有记录则减计数并返回 false，无记录返回 true（应删除文件）
     * @param filePath 目标文件路径
     */
    bool decrement(const std::string& filePath);

private:
    JsonFile m_json;
};
