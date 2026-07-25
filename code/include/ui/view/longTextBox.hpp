/**
 * LongTextBox - 长文本内容 Box 生成模块
 *
 * 调用方先通过 LongTextBoxConfig 组织多组标题、正文和二维码，
 * 再调用 LongTextBox::create() 一次性生成 Box。
 *
 * 用法：
 *   LongTextBoxConfig content;
 *
 *   auto& a = content.addEntry();
 *   a.addTitle("标题 A");
 *   a.addBody("正文 A");
 *   a.addQr(qrA, "标签 A");
 *
 *   auto& b = content.addEntry();
 *   b.addTitle("标题 B");
 *   b.addBody("正文 B");
 *
 *   auto* box = LongTextBox::create(content);
 */

#pragma once

#include <borealis.hpp>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

/** @brief 长文本内容中的二维码项目 */
struct LongTextQrItem {
    std::string url;   // 二维码内容
    std::string label; // 二维码下方标签，空字符串则不显示
};

/** @brief 一组长文本内容 */
struct LongTextEntry {
    static constexpr std::size_t MAX_QR_COUNT = 3; // 每组最多显示的二维码数量

    std::string title;                     // 标题，空字符串则不显示
    std::string body;                      // 正文，空字符串则不显示
    float bodyLineHeight;                  // 正文行高，由 addBody() 设置
    std::vector<LongTextQrItem> qrItems;   // 二维码项目列表

    /**
     * @brief 设置标题
     * @param value 标题文本
     * @return 当前内容组
     */
    LongTextEntry& addTitle(const std::string& value) {
        title = value;
        return *this;
    }

    /**
     * @brief 设置正文
     * @param value 正文内容
     * @param lineHeight 行间距，默认为 1.65
     * @return 当前内容组
     */
    LongTextEntry& addBody(const std::string& value, float lineHeight = 1.65f) {
        body = value;
        bodyLineHeight = lineHeight;
        return *this;
    }

    /**
     * @brief 添加二维码
     * @param url 二维码内容
     * @param label 二维码下方标签，为空时不显示
     * @return 当前内容组
     */
    LongTextEntry& addQr(const std::string& url, const std::string& label = "") {
        if (qrItems.size() < MAX_QR_COUNT) qrItems.push_back({url, label});
        return *this;
    }
};

/** @brief 长文本内容配置 */
struct LongTextBoxConfig {
    std::deque<LongTextEntry> entries; // 内容组列表，保证已返回引用的稳定性

    /**
     * @brief 添加内容组并返回稳定引用
     * @return 新添加的内容组
     */
    LongTextEntry& addEntry() {
        entries.emplace_back();
        return entries.back();
    }
};

/** @brief 根据配置生成长文本内容容器 */
class LongTextBox {
public:
    /**
     * @brief 根据内容配置生成 Box
     * @param content 内容配置，仅在方法执行期间读取
     * @return 生成的 Box，所有权交给调用方
     */
    static brls::Box* create(const LongTextBoxConfig& content);
};
