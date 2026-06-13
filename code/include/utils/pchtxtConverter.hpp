/**
 * PchtxtConverter - pchtxt 文本补丁转 IPS32 二进制
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace PchtxtConverter {

struct Result {
    bool success = false;
    std::string errorMsg;           // 失败时：错误原因
    std::string nsobid;             // 解析到的 NSO Build ID（即输出文件名）
    std::vector<uint8_t> ipsData;   // 生成的 IPS32 二进制数据
};

/**
 * @brief 解析 pchtxt 文件并生成 IPS32 二进制数据
 * - 从首行提取 @nsobid 作为输出文件名
 * - 解析所有 @enabled 状态下的补丁行（字节模式和字符串模式）
 * - 支持 @flag offset_shift 偏移补偿
 * - 格式错误时返回带行号的错误信息
 * - 调用方负责将 ipsData 写入目标路径（如 exefs_patches/{mod名}/{nsobid}.ips）
 * @param data pchtxt 文件数据指针
 * @param size 数据大小
 */
Result convert(const void* data, size_t size);

}
