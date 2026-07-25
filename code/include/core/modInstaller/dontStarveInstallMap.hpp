/**
 * DontStarveInstallMap - 饥荒特殊 MOD 安装映射记录
 */

#pragma once

#include <string>
#include <vector>
#include "utils/jsonFile.hpp"

class DontStarveInstallMap {
public:
    /** @brief 创建空的饥荒安装映射 */
    DontStarveInstallMap() = default;

    /**
     * @brief 加载指定游戏的饥荒安装映射文件
     * @param path 映射文件路径
     * @return 是否加载成功
     */
    bool load(const std::string& path);

    /**
     * @brief 持久化到文件
     * @return 是否保存成功
     */
    bool save();

    /**
     * @brief 指定 MOD 是否存在饥荒目录映射
     * @param modDirName MOD 目录名
     * @return 是否存在映射
     */
    bool hasMappings(const std::string& modDirName);

    /**
     * @brief 获取全部实际安装目录名
     * @return 实际安装目录名列表
     */
    std::vector<std::string> allTargetDirs();

    /**
     * @brief 读取指定源目录实际安装后的目标目录名
     * @param modDirName MOD 目录名
     * @param sourceDir 源目录名
     * @return 实际安装后的目标目录名
     */
    std::string targetDir(const std::string& modDirName, const std::string& sourceDir);

    /**
     * @brief 写入或更新指定 MOD 的饥荒目录映射
     * @param modDirName MOD 目录名
     * @param sourceDir 源目录名
     * @param targetDir 目标目录名
     */
    void setMapping(const std::string& modDirName, const std::string& sourceDir, const std::string& targetDir);

    /**
     * @brief 删除指定 MOD 的全部饥荒目录映射
     * @param modDirName MOD 目录名
     */
    void removeMod(const std::string& modDirName);

private:
    JsonFile m_json; // 映射文件数据
};
