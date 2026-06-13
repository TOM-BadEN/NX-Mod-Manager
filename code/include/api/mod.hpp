/**
 * api/mod - 模组相关 API 请求
 * 封装 HTTP 请求 + JSON 解析，返回干净的业务数据
 */

#pragma once

#include "api/apiResult.hpp"
#include <string>
#include <vector>
#include <stop_token>

namespace api::mod {

// 商店模组列表中的单个模组条目
struct ModList {
    int modId = 0;                  // 模组 ID
    std::string modName;            // 模组名称（多语言，由服务器根据 lang 参数返回）
    std::string modType;            // 功能类型（performance/graphics/translation 等）
    std::string modVersion;         // 模组版本
    std::string gameVersion;        // 适配的游戏版本
    std::string author;             // 作者
    std::string authorLink;         // 作者链接
    int downloadCount = 0;          // 下载量
    int likeCount = 0;              // 点赞数
    int dislikeCount = 0;           // 踩数
    std::string uploadTime;         // 上传时间（格式：YYYY-MM-DD HH:MM:SS）
    std::string fileCrc32;          // MOD 文件 CRC32（服务端返回，用于后续更新判断）
    bool downloaded = false;        // 是否已从商店下载（客户端本地填充）
    bool hasUpdate = false;         // 是否有可用更新（客户端本地填充）
    bool installed = false;         // 是否已安装到游戏中（客户端本地填充）
};

// 获取商店模组列表的结果
struct ModListResult : api::ApiResult {
    std::string gameTid;              // 游戏 TID
    std::string gameName;             // 游戏名称（多语言）
    int total = 0;                    // 服务器上的模组总数（用于判断是否还有更多数据）
    std::vector<ModList> list;        // 当前页的模组列表
};

/**
 * 获取商店模组列表
 * @param gameTid 游戏 TID
 * @param page 页码（从 1 开始）
 * @param limit 每页数量
 * @param sort 排序方式（latest/download/like）
 * @param keyword 搜索关键词（空字符串表示不搜索）
 * @param version 版本筛选（空字符串表示不筛选）
 * @param modType 类型筛选（空字符串表示不筛选）
 * @param token 用于取消请求的停止令牌
 * @return ModListResult 包含成功/失败状态和模组列表
 */
ModListResult fetchModList(const std::string& gameTid,
                           int page = 1, int limit = 20,
                           const std::string& sort = "latest",
                           const std::string& keyword = "",
                           const std::string& version = "",
                           const std::string& modType = "",
                           std::stop_token token = {});

// 获取模组适配游戏版本列表的结果
struct ModGameVersionsResult : api::ApiResult {
    std::vector<std::string> versions;  // 版本号列表（"0" 通用版本由客户端手动添加）
};

/**
 * 获取指定游戏下模组所适配的游戏版本列表
 * @param gameTid 游戏 TID
 * @param token 用于取消请求的停止令牌
 * @return ModGameVersionsResult 包含成功/失败状态和版本列表
 */
ModGameVersionsResult fetchModGameVersions(const std::string& gameTid, std::stop_token token = {});

// 模组详情数据
struct ModDetail {
    int modId = 0;                  // 模组 ID
    std::string gameTid;            // 游戏 TID
    std::string gameName;           // 游戏名称（多语言）
    std::string modName;            // 模组名称（多语言）
    std::string modType;            // 功能类型（performance/graphics/translation 等）
    std::string modVersion;         // 模组版本
    std::string gameVersion;        // 适配的游戏版本
    std::string author;             // 作者
    std::string authorLink;         // 作者链接
    std::string description;        // 模组描述（多语言）
    int fileSize = 0;               // 文件大小（字节）
    std::string uploadTime;         // 上传时间（格式：YYYY-MM-DD HH:MM:SS）
    int downloadCount = 0;          // 下载量
    int likeCount = 0;              // 点赞数
    int dislikeCount = 0;           // 踩数
    bool deviceLiked = false;       // 当前设备是否已点赞
    bool deviceDisliked = false;    // 当前设备是否已点踩
    bool downloaded = false;        // 是否已从商店下载（客户端本地填充）
    bool hasUpdate = false;         // 是否有可用更新（客户端本地填充）
    bool installed = false;         // 是否已安装到游戏中（客户端本地填充）
    std::string fileCrc32;          // MOD 文件 CRC32（8 位十六进制小写字符串，用于与服务端比对文件是否更新）
    std::string changelog;          // 更新日志（多语言）
};

// 获取模组详情的结果
struct ModDetailResult : api::ApiResult {
    ModDetail detail;
};

/**
 * 获取模组详情
 * @param modId 模组 ID
 * @param token 用于取消请求的停止令牌
 * @return ModDetailResult 包含成功/失败状态和模组详情
 */
ModDetailResult fetchModDetail(int modId, std::stop_token token = {});

/**
 * 切换模组点赞状态
 * @param modId 模组 ID
 * @param token 用于取消请求的停止令牌
 * @return ApiResult 包含成功/失败状态和错误信息
 */
api::ApiResult toggleLike(int modId, std::stop_token token = {});

/**
 * 切换模组点踩状态
 * @param modId 模组 ID
 * @param token 用于取消请求的停止令牌
 * @return ApiResult 包含成功/失败状态和错误信息
 */
api::ApiResult toggleDislike(int modId, std::stop_token token = {});

// 下载信息（英文名称，用于目录命名）
struct DownloadInfoResult : api::ApiResult {
    std::string gameNameEn;     // 英文游戏名（可能为空）
    std::string modNameEn;      // 英文模组名（可能为空）
};

/**
 * 获取模组下载信息（英文游戏名 + 英文模组名）
 * @param modId 模组 ID
 * @param token 用于取消请求的停止令牌
 * @return DownloadInfoResult 包含成功/失败状态和英文名称
 */
DownloadInfoResult fetchDownloadInfo(int modId, std::stop_token token = {});

// 模组更新检查结果
struct ModUpdateCheckResult : api::ApiResult {
    std::vector<int> updatedModIds;  // 有更新的 mod ID 列表
};

/**
 * 检查已安装模组是否有更新
 * @param gameTid 游戏 TID
 * @param modsJson JSON 字符串，格式 {"123":"a1b2c3d4","456":"ffff0000"}
 * @param token 用于取消请求的停止令牌
 * @return ModUpdateCheckResult 包含成功/失败状态和更新的 mod ID 列表
 */
ModUpdateCheckResult checkModUpdates(const std::string& gameTid, const std::string& modsJson, std::stop_token token = {});

// 截图下载结果
struct ScreenshotResult : api::ApiResult {
    std::vector<uint8_t> data;  // webp 原始数据
};

/**
 * 下载模组截图（镜像加速优先，失败回退源站）
 * @param gameTid 游戏 TID
 * @param modId 模组 ID
 * @param index 截图序号（1 或 2）
 * @param token 用于取消请求的停止令牌
 * @return ScreenshotResult 包含成功/失败状态和截图数据
 */
ScreenshotResult fetchScreenshot(const std::string& gameTid, int modId, int index, std::stop_token token = {});

} // namespace api::mod
