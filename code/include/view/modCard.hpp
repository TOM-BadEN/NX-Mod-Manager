/**
 * ModCard - Mod 卡片组件
 * 显示单个 mod 的图标、名称、类型和安装状态
 */

#pragma once

#include "view/recyclingGrid.hpp"
#include "view/svgImage.hpp"

class ModCard : public RecyclingGridItem {
public:
    ModCard();
    
    /**
     * @brief 设置卡片数据
     * @param name 模组名称
     * @param type 类型
     * @param installed 是否已安装
     * @param disabled 游戏是否处于模组禁用状态
     * @param modID 模组 ID
     */
    void setMod(const std::string& name, const std::string& type, bool installed, bool disabled, int modID = -1);
    
    /** @brief 回收复用时重置内容 */
    void prepareForReuse() override;

    /** @brief 工厂函数 */
    static RecyclingGridItem* create();

private:
    BRLS_BIND(brls::Image, m_icon, "modCard/icon");
    BRLS_BIND(brls::Label, m_name, "modCard/name");
    BRLS_BIND(brls::Label, m_type, "modCard/type");
    BRLS_BIND(brls::Label, m_status, "modCard/status");
    BRLS_BIND(SVGImage, m_cloudIcon, "modCard/cloudIcon");
};
