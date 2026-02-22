/**
 * Home - 主页面
 * 
 * Activity 是 Borealis 的页面容器，每个页面对应一个 Activity。
 * 页面的 UI 布局定义在 XML 文件中，通过 CONTENT_FROM_XML_RES 宏绑定。
 */

#pragma once
#include <borealis.hpp>
#include <vector>
#include <atomic>
#include "view/recyclingGrid.hpp"
#include "view/myframe.hpp"
#include "common/gameInfo.hpp"
#include "utils/jsonFile.hpp"
#include "utils/async.hpp"
#include "view/actionMenu.hpp"

struct GameMetadata;

class Home : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/home.xml");
    
    // 绑定组件
    BRLS_BIND(MyFrame, m_frame, "home/frame");
    BRLS_BIND(RecyclingGrid, m_grid, "home/grid");
    BRLS_BIND(brls::Label, m_noModHint, "home/noModHint");
    
    // XML 加载完成后调用
    void onContentAvailable() override;

private:
    std::vector<GameInfo> m_games;                              // 游戏列表（第一阶段填充，第二阶段更新）
    JsonFile m_jsonCache;                                       // JSON 缓存（gameInfo.json）
    util::AsyncFurture<void> m_nacpLoader;                      // 异步 NACP 加载任务
    std::atomic<int> m_focusedIndex{0};                          // 当前焦点索引（后台线程读，主线程写）
    bool m_sortAsc = true;                                      // 排序方向（true=升序）

    void showEmptyHint();                                       // 显示空列表提示
    void setupGridPage();                                       // 初始化网格 + 注册回调
    void startNacpLoader();                                     // 启动异步 NACP 加载
    void toggleSort();                                          // 切换排序方向
    void flipScreen(int direction);                             // LB/RB 翻页
    void applyMetadata(int gameIdx, const GameMetadata& meta);  // 应用 NACP 数据到游戏 + UI


    // 临时菜单测试变量
    bool m_toggleState = false;
    int m_currentMode = 1;             // 当前模式
    
    MenuPageConfig m_testMenu;                                   // 测试菜单
    MenuPageConfig m_modeSubMenu;      // 子菜单配置
    MenuPageConfig m_multiSelectTest;  // 多选测试子菜单
    
};
