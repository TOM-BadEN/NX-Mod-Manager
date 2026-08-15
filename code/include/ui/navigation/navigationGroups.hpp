/**
 * NavigationGroups - 全局标题栏的公共导航组配置
 *
 * 不同页面共用的导航组统一定义在这里，页面只需要指定当前所在位置。
 */

#pragma once

#include "ui/core/headerState.hpp"

/** @brief 主导航中的页面位置 */
enum class MainNavigationPage {
    StoreGameList,
    Home,
    AddGame,
};

/** @brief Mod 导航中的页面位置 */
enum class ModNavigationPage {
    StoreModList,
    ModList,
    StoreModDetail,
};

/**
 * @brief 创建主导航标题状态
 * @param selectedPage 当前页面
 * @return 包含按键提示、页面图标和选中位置的导航状态
 */
NavigationState createMainNavigationState(MainNavigationPage selectedPage);

/**
 * @brief 创建 Mod 导航标题状态
 * @param selectedPage 当前页面
 * @return 包含按键提示、页面图标和选中位置的导航状态
 */
NavigationState createModNavigationState(ModNavigationPage selectedPage);
