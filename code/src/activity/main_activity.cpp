/**
 * 主页面实现文件
 */

#include "activity/main_activity.hpp"
#include "scanner/gameScanner.hpp"

void MainActivity::onContentAvailable() {
    GameScanner scanner;
    auto games = scanner.scanGames();
    m_gridPage->setGameList(games);
    
    // 设置索引回调，更新 Footer 索引显示
    m_gridPage->setIndexChangeCallback([this](int index, int total) {
        m_frame->setIndexText(std::to_string(index) + " / " + std::to_string(total));
    });
}
