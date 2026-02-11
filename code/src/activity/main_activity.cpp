/**
 * 主页面实现文件
 */

#include "activity/main_activity.hpp"

void MainActivity::onContentAvailable()
{
    // 填充测试数据
    std::vector<GameInfo> games;
    for (int i = 0; i < 20; i++)
    {
        GameInfo info;
        info.name = "测试游戏 " + std::to_string(i + 1);
        info.version = "1.0." + std::to_string(i);
        info.modCount = std::to_string(i * 2 + 1);
        games.push_back(info);
    }
    m_gridPage->setGameList(games);
}
