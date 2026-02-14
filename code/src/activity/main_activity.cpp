/**
 * 主页面实现文件
 */

#include "activity/main_activity.hpp"
#include "scanner/gameScanner.hpp"

void MainActivity::onContentAvailable() {
    GameScanner scanner;
    auto games = scanner.scanGames();
    
    if (games.empty()) {
        m_gridPage->setVisibility(brls::Visibility::GONE);
        m_noModHint->setVisibility(brls::Visibility::VISIBLE);
        m_noModHint->setFocusable(true);
        m_noModHint->setHideHighlight(true);
        brls::Application::giveFocus(m_noModHint);
        return;
    }
    
    m_gridPage->setGameList(games);
    // 设置索引回调，更新 Footer 索引显示
    m_gridPage->setIndexChangeCallback([this](int index, int total) {
        m_frame->setIndexText(std::to_string(index) + " / " + std::to_string(total));
    });
}
