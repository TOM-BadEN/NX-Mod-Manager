/**
 * GameCard - 游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 mod 数量
 */

#pragma once

#include <borealis.hpp>

class GameCard : public brls::Box {
public:
    GameCard();
    
    // 设置卡片数据
    void setGame(const std::string& name, const std::string& version, const std::string& modCount);
    
    // 设置游戏图标（传入 NVG image ID）
    void setIcon(int imageId);
    
    // 清空卡片（隐藏，图标恢复默认）
    void clear();
    
    // 工厂函数
    static brls::View* create();

private:
    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "gameCard/icon");
    BRLS_BIND(brls::Label, m_name, "gameCard/name");
    BRLS_BIND(brls::Label, m_version, "gameCard/version");
    BRLS_BIND(brls::Label, m_modCount, "gameCard/modCount");
};
