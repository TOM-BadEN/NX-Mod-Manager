/**
 * AddGameCard - 新增游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 MOD 数量（黑绿配色）
 */

#pragma once

#include "ui/view/recyclingGrid.hpp"
#include <borealis.hpp>
#include <string>

class AddGameCard : public RecyclingGridItem {
public:
    /** @brief 创建新增游戏卡片并加载 XML 布局 */
    AddGameCard();

    /**
     * @brief 设置卡片数据
     * @param name 游戏名称
     * @param version 版本号
     * @param modCount MOD 数量
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

    /**
     * @brief 创建可供 RecyclingGrid 使用的新增游戏卡片
     * @return 新创建的新增游戏卡片
     */
    static RecyclingGridItem* create();

private:
    int m_defaultIconId = 0; // 默认游戏图标的纹理 ID

    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "addGameCard/icon");         // 游戏图标
    BRLS_BIND(brls::Image, m_like, "addGameCard/like");         // 收藏图标
    BRLS_BIND(brls::Label, m_name, "addGameCard/name");         // 游戏名称
    BRLS_BIND(brls::Label, m_version, "addGameCard/version");   // 游戏版本
    BRLS_BIND(brls::Label, m_modCount, "addGameCard/modCount"); // MOD 数量
};
