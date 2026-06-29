/**
 * StoreModCard - 商店模组卡片组件
 * 显示单个模组的图标、名称、标签（版本/作者/类型/适配版本）和统计数据
 */

#pragma once

#include <borealis.hpp>
#include "view/recyclingGrid.hpp"
#include "view/capsuleBadge.hpp"

class StoreModCard : public RecyclingGridItem {
public:
    StoreModCard();

    /**
     * @brief 设置卡片数据
     * @param name 模组名称
     * @param uploadTime 上传时间
     * @param type 类型
     * @param gameVersion 适配版本
     * @param likes 点赞数
     * @param dislikes 点踩数
     * @param downloads 下载数
     * @param downloaded 是否已下载
     * @param hasUpdate 是否有可用更新
     */
    void setMod(const std::string& name,
                const std::string& uploadTime,
                const std::string& type,
                const std::string& gameVersion,
                int likes, int dislikes, int downloads,
                bool downloaded,
                bool hasUpdate = false);

    /** @brief 回收时重置状态 */
    void prepareForReuse() override;

    /** @brief 工厂函数 */
    static RecyclingGridItem* create();

private:
    // XML 绑定的组件
    BRLS_BIND(brls::Image, m_icon, "storeModCard/icon");
    BRLS_BIND(brls::Label, m_name, "storeModCard/name");
    BRLS_BIND(brls::Image, m_installedIcon, "storeModCard/installedIcon");
    BRLS_BIND(brls::Box, m_updateBadge, "storeModCard/updateBadge");
    BRLS_BIND(CapsuleBadge, m_tagType, "storeModCard/tagType");
    BRLS_BIND(CapsuleBadge, m_tagGameVersion, "storeModCard/tagGameVersion");
    BRLS_BIND(brls::Image, m_iconLike, "storeModCard/iconLike");
    BRLS_BIND(brls::Image, m_iconDislike, "storeModCard/iconDislike");
    BRLS_BIND(brls::Image, m_iconDownload, "storeModCard/iconDownload");
    BRLS_BIND(brls::Image, m_iconTime, "storeModCard/iconTime");
    BRLS_BIND(brls::Label, m_statLike, "storeModCard/statLike");
    BRLS_BIND(brls::Label, m_statDislike, "storeModCard/statDislike");
    BRLS_BIND(brls::Label, m_statDownload, "storeModCard/statDownload");
    BRLS_BIND(brls::Label, m_statTime, "storeModCard/statTime");
};
