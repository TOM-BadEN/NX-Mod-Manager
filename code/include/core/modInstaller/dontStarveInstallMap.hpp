/**
 * DontStarveInstallMap - 饥荒特殊 MOD 安装映射记录
 */

#pragma once

#include <string>
#include <vector>
#include "utils/jsonFile.hpp"

class DontStarveInstallMap {
public:
    DontStarveInstallMap() = default;

    /**
     * @brief 加载指定游戏的饥荒安装映射文件
     * @param path 映射文件路径
     */
    bool load(const std::string& path);

    /** @brief 持久化到文件 */
    bool save();

    /** @brief 指定 MOD 是否存在饥荒目录映射 */
    bool hasMappings(const std::string& modDirName);

    /** @brief 获取全部实际安装目录名 */
    std::vector<std::string> allTargetDirs();

    /**
     * @brief 读取指定源目录实际安装后的目标目录名
     */
    std::string targetDir(const std::string& modDirName, const std::string& sourceDir);

    /**
     * @brief 写入或更新指定 MOD 的饥荒目录映射
     */
    void setMapping(const std::string& modDirName, const std::string& sourceDir, const std::string& targetDir);

    /**
     * @brief 删除指定 MOD 的全部饥荒目录映射
     */
    void removeMod(const std::string& modDirName);

private:
    JsonFile m_json;
};
