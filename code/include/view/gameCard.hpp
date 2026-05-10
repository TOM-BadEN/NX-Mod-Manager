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
    
    /**
     * @brief 设置卡片数据
     * @param name 游戏名称
     * @param version 版本号
     * @param modCount mod 数量
     */
    void setGame(const std::string& name, const std::string& version, const std::string& modCount);
    
    /**
     * @brief 设置游戏图标
     * @param iconId NVG 纹理 ID
     */
    void setIcon(int iconId);
    
    /**
     * @brief 设置收藏状态
     * @param favorite 是否收藏
     */
    void setFavorite(bool favorite);

    /**
     * @brief 设置游戏名文字颜色
     * @param color 颜色
     */
    void setNameColor(NVGcolor color);
    
    /** @brief 恢复默认图标 */
    void resetIcon();

    /** @brief 回收时重置状态 */
    void prepareForReuse() override;

    /** @brief 工厂函数 */
    static RecyclingGridItem* create();

private:
    int m_defaultIconId = 0;

    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "gameCard/icon");
    BRLS_BIND(brls::Image, m_like, "gameCard/like");
    BRLS_BIND(brls::Label, m_name, "gameCard/name");
    BRLS_BIND(brls::Label, m_version, "gameCard/version");
    BRLS_BIND(brls::Label, m_modCount, "gameCard/modCount");
};
