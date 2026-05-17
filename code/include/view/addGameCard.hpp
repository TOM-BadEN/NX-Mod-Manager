/**
 * AddGameCard - 新增游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 mod 数量（黑绿配色）
 */

#pragma once

#include <borealis.hpp>
#include "view/recyclingGrid.hpp"
#include "view/capsuleBadge.hpp"

class AddGameCard : public RecyclingGridItem {
public:
    AddGameCard();
    
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

    /** @brief 恢复默认图标 */
    void resetIcon();

    /** @brief 回收时重置状态 */
    void prepareForReuse() override;

    /** @brief 工厂函数 */
    static RecyclingGridItem* create();

private:
    int m_defaultIconId = 0;

    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "addGameCard/icon");
    BRLS_BIND(brls::Image, m_like, "addGameCard/like");
    BRLS_BIND(brls::Label, m_name, "addGameCard/name");
    BRLS_BIND(CapsuleBadge, m_version, "addGameCard/version");
    BRLS_BIND(CapsuleBadge, m_modCount, "addGameCard/modCount");
};
