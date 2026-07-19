/**
 * MHRiseInstallMap - 怪猎特殊 pak 安装映射记录
 */

#pragma once

#include <string>
#include <vector>
#include "utils/jsonFile.hpp"

struct MHRisePakMapping {
    std::string modDirName;
    std::string sourcePak;
    std::string targetPak;
};

class MHRiseInstallMap {
public:
    MHRiseInstallMap() = default;

    /**
     * @brief 加载指定游戏的怪猎安装映射文件
     * @param path 映射文件路径
     */
    bool load(const std::string& path);

    /** @brief 持久化到文件 */
    bool save();

    /** @brief 读取安装时游戏版本 */
    std::string gameVersion();

    /** @brief 是否存在怪猎特殊 pak 安装映射记录 */
    bool hasRecords();

    /** @brief 已安装怪猎特殊 MOD 数量 +1 */
    void increaseInstalledCount();

    /** @brief 已安装怪猎特殊 MOD 数量 -1，最低保持 0 */
    void decreaseInstalledCount();

    /** @brief 读取最后已记录的 patch 编号 */
    int lastPatchNo();

    /** @brief 写入最后已记录的 patch 编号 */
    void setLastPatchNo(int patchNo);

    /**
     * @brief 写入安装时游戏版本
     * @param version 格式化后的游戏版本
     */
    void setGameVersion(const std::string& version);

    /**
     * @brief 获取指定 MOD 的所有特殊 pak 映射
     * @param modDirName MOD 目录名
     */
    std::vector<MHRisePakMapping> mappings(const std::string& modDirName);

    /** @brief 获取全部特殊 pak 映射 */
    std::vector<MHRisePakMapping> allMappings();

    /**
     * @brief 读取指定源 pak 实际安装后的目标 pak 文件名
     */
    std::string targetPak(const std::string& modDirName, const std::string& sourcePak);

    /**
     * @brief 写入或更新指定 MOD 的特殊 pak 映射
     */
    void setMapping(const std::string& modDirName, const std::string& sourcePak, const std::string& targetPak);

    /**
     * @brief 删除指定 MOD 的某个特殊 pak 映射
     */
    void removeMapping(const std::string& modDirName, const std::string& sourcePak);

    /**
     * @brief 删除指定 MOD 的全部特殊 pak 映射
     */
    void removeMod(const std::string& modDirName);

private:
    JsonFile m_json;
};
