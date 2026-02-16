/**
 * ModCard - Mod 卡片组件
 * 显示单个 mod 的图标、名称、类型和安装状态
 */

#pragma once

#include <borealis.hpp>

class ModCard : public brls::Box {
public:
    ModCard();
    
    // 设置卡片数据
    void setMod(const std::string& name, const std::string& type, bool installed);
    
    // 清空卡片（隐藏）
    void clear();
    
    // 工厂函数
    static brls::View* create();

private:
    BRLS_BIND(brls::Image, m_icon, "modCard/icon");
    BRLS_BIND(brls::Label, m_name, "modCard/name");
    BRLS_BIND(brls::Label, m_type, "modCard/type");
    BRLS_BIND(brls::Label, m_status, "modCard/status");
};
