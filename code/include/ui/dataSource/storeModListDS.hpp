/**
 * StoreModListDS - 商店模组列表数据源
 * 将 ModList 数据绑定到 RecyclingGrid 的 StoreModCard Cell
 */

#pragma once

#include "api/mod.hpp"
#include "ui/view/recyclingGrid.hpp"
#include "ui/view/storeModCard.hpp"
#include <functional>
#include <utility>
#include <vector>

class StoreModListDS : public RecyclingGridDataSource {
public:
    /**
     * @brief 构造商店模组列表数据源
     * @param storeModList 模组列表引用
     * @param skeletonCount 模组列表为空时显示的轻量骨架数量
     * @param clickCallback 卡片点击回调
     */
    StoreModListDS(std::vector<api::mod::ModList>& storeModList, size_t skeletonCount, std::function<void(size_t)> clickCallback)
        : m_storeModList(storeModList), m_skeletonCount(skeletonCount), m_clickCallback(std::move(clickCallback)) {}

    /** @brief 返回模组总数 */
    size_t getItemCount() override { return m_storeModList.empty() ? m_skeletonCount : m_storeModList.size(); }

    /**
     * @brief 取一个空卡片，填上第 index 个模组的数据
     * @param grid 网格容器
     * @param index 模组索引
     */
    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        if (m_storeModList.empty()) return grid->dequeueReusableCell("Skeleton");

        auto& mod = m_storeModList[index];
        if (mod.isPending) return grid->dequeueReusableCell("Skeleton");

        auto* card = static_cast<StoreModCard*>(grid->dequeueReusableCell("StoreModCard"));
        card->setMod(mod.modName, mod.uploadTime, mod.modType, mod.gameVersion, mod.likeCount, mod.dislikeCount, mod.downloadCount, mod.downloaded, mod.hasUpdate);
        return card;
    }

    /**
     * @brief 用户点击卡片时触发
     * @param grid 网格容器
     * @param index 模组索引
     */
    void onItemSelected(RecyclingGrid* grid, size_t index) override {
        if (m_clickCallback) m_clickCallback(index);
    }

    /** @brief 清空数据（空实现） */
    void clearData() override {}

private:
    std::vector<api::mod::ModList>& m_storeModList; // 模组列表引用（与 StoreModManager 共享）
    size_t m_skeletonCount;                          // 模组列表为空时显示的轻量骨架数量
    std::function<void(size_t)> m_clickCallback;     // 卡片点击回调
};
