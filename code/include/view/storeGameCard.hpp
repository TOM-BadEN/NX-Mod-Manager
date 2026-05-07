/**
 * StoreGameCard - 商店游戏卡片组件
 * 显示单个游戏的图标、名称、MOD数量和更新时间
 */

#pragma once

#include <borealis.hpp>
#include "view/recyclingGrid.hpp"

class StoreGameCard : public RecyclingGridItem {
public:
    StoreGameCard();

    /**
     * @brief 设置卡片数据
     * @param name 游戏名称
     * @param modCount MOD 数量
     * @param lastUpdate 最后更新时间
     */
    void setGame(const std::string& name, const std::string& modCount, const std::string& lastUpdate);

    /**
     * @brief 设置游戏图标
     * @param iconId NVG 纹理 ID
     */
    void setIcon(int iconId);

    /**
     * @brief 设置已安装状态
     * @param installed 是否已安装
     */
    void setInstalled(bool installed);

    /** @brief 回收时重置状态 */
    void prepareForReuse() override;

    /** @brief 工厂函数 */
    static RecyclingGridItem* create();

private:
    int m_defaultIconId = 0;

    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "storeGameCard/icon");
    BRLS_BIND(brls::Image, m_installed, "storeGameCard/installed");
    BRLS_BIND(brls::Label, m_name, "storeGameCard/name");
    BRLS_BIND(brls::Label, m_modCount, "storeGameCard/modCount");
    BRLS_BIND(brls::Label, m_lastUpdate, "storeGameCard/lastUpdate");
};
