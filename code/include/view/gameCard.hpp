/**
 * GameCard - 游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 mod 数量
 */

#pragma once

#include <borealis.hpp>
#include "view/recyclingGrid.hpp"

class GameCard : public RecyclingGridItem {
public:
    GameCard();
    
    // 设置卡片数据
    void setGame(const std::string& name, const std::string& version, const std::string& modCount);
    
    // 设置游戏图标（传入 NVG 纹理 ID）
    void setIcon(int iconId);
    
    // 恢复默认图标（不走缓存计数）
    void resetIcon();

    // 回收时重置状态
    void prepareForReuse() override;
    
    // 工厂函数
    static RecyclingGridItem* create();

private:
    int m_defaultIconId = 0;

    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "gameCard/icon");
    BRLS_BIND(brls::Label, m_name, "gameCard/name");
    BRLS_BIND(brls::Label, m_version, "gameCard/version");
    BRLS_BIND(brls::Label, m_modCount, "gameCard/modCount");
};
