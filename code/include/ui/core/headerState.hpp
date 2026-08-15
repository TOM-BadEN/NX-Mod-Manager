/**
 * HeaderState - 全局标题栏的显示状态
 *
 * 只描述标题栏需要显示的数据，不负责页面切换、输入处理或具体绘制。
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/** @brief 普通标题的显示状态 */
struct TitleState {
    std::string iconPath;        // 页面图标路径，空字符串表示不使用
    std::string iconTextureKey;  // 页面图标纹理缓存 Key，空字符串表示不使用
    std::string title;           // 主标题
    std::string subtitle;        // 副标题，空字符串表示不显示

    bool operator==(const TitleState&) const = default;
};

/** @brief 导航标题的显示状态 */
struct NavigationState {
    std::string leftButton;                   // 左侧按钮
    std::vector<std::string> pageIconPaths;   // 按显示顺序排列的页面图标路径
    std::size_t selectedPageIndex = 0;        // 当前选中的页面位置
    std::string rightButton;                  // 右侧按钮

    bool operator==(const NavigationState&) const = default;
};

/** @brief 顶部标题区域的完整显示状态 */
struct HeaderState {
    std::optional<NavigationState> navigation; // 导航标题，未提供时不显示
    std::optional<TitleState> title;           // 普通标题，未提供时不显示
    std::optional<std::string> contentTitle;   // 内容标题，未提供时不显示

    /** @brief 设置导航标题 */
    void setNavigation(NavigationState state) {
        navigation = std::move(state);
    }

    /** @brief 设置普通标题 */
    void setTitle(TitleState state) {
        title = std::move(state);
    }

    /** @brief 设置仅包含主标题文字的普通标题 */
    void setTitle(std::string title) {
        TitleState state;
        state.title = std::move(title);
        setTitle(std::move(state));
    }

    /** @brief 设置内容标题 */
    void setContentTitle(std::string title) {
        contentTitle = std::move(title);
    }

    bool operator==(const HeaderState&) const = default;
};
