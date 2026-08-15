/**
 * NavigationGroups - 全局标题栏的公共导航组配置
 */

#include "ui/navigation/navigationGroups.hpp"
#include "utils/format.hpp"
#include <borealis.hpp>
#include <borealis/views/hint.hpp>

NavigationState createMainNavigationState(MainNavigationPage selectedPage) {
    NavigationState state;
    state.leftButton = brls::Hint::getKeyIcon(brls::BUTTON_LT);
    state.pageIconPaths = {
        format::themedIconPath("img/page/store"),
        format::themedIconPath("img/page/home"),
        format::themedIconPath("img/page/add"),
    };
    state.selectedPageIndex = static_cast<std::size_t>(selectedPage);
    state.rightButton = brls::Hint::getKeyIcon(brls::BUTTON_RT);
    return state;
}

NavigationState createModNavigationState(ModNavigationPage selectedPage) {
    NavigationState state;
    state.leftButton = brls::Hint::getKeyIcon(brls::BUTTON_LT);
    state.pageIconPaths = {
        format::themedIconPath("img/page/storeModList"),
        format::themedIconPath("img/page/modList"),
        format::themedIconPath("img/page/storeModDetail"),
    };
    state.selectedPageIndex = static_cast<std::size_t>(selectedPage);
    state.rightButton = brls::Hint::getKeyIcon(brls::BUTTON_RT);
    return state;
}
