/**
 * 主页面（倒计时页面）头文件
 * 
 * Activity 是 Borealis 的页面容器，每个页面对应一个 Activity。
 * 页面的 UI 布局定义在 XML 文件中，通过 CONTENT_FROM_XML_RES 宏绑定。
 */

#pragma once
#include <borealis.hpp>
#include <vector>
#include <atomic>
#include "view/gridPage.hpp"
#include "view/myframe.hpp"
#include "common/gameInfo.hpp"
#include "utils/jsonFile.hpp"
#include "utils/async.hpp"

struct GameMetadata;

class MainActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/main.xml");
    
    // 绑定组件
    BRLS_BIND(MyFrame, m_frame, "main/frame");
    BRLS_BIND(GridPage, m_gridPage, "main/gridPage");
    BRLS_BIND(brls::Label, m_noModHint, "main/noModHint");
    
    // XML 加载完成后调用
    void onContentAvailable() override;

private:
    std::vector<GameInfo> m_games;                              // 游戏列表（第一阶段填充，第二阶段更新）
    JsonFile m_jsonCache;                                       // JSON 缓存（gameInfo.json）
    util::AsyncFurture<void> m_nacpLoader;                      // 异步 NACP 加载任务
    std::atomic<int> m_currentPage{0};                          // 当前页码（后台线程读，主线程写）

    void showEmptyHint();                                       // 显示空列表提示
    void setupGridPage();                                       // 初始化九宫格 + 注册回调
    void startNacpLoader();                                     // 启动异步 NACP 加载
    void applyMetadata(int gameIdx, const GameMetadata& meta);  // 应用 NACP 数据到游戏 + UI
};
