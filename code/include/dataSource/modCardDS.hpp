/**
 * ModCardDS - Mod 卡片数据源
 * 将 m_mods 数据绑定到 RecyclingGrid 的 ModCard Cell
 */

#pragma once

#include "view/recyclingGrid.hpp"
#include "view/modCard.hpp"
#include "common/modInfo.hpp"
#include <vector>
#include <functional>

class ModCardDS : public RecyclingGridDataSource {
public:
    /**
     * @brief 构造 Mod 卡片数据源
     * @param mods mod 列表引用
     * @param disabled 游戏模组禁用状态引用
     * @param clickCallback 卡片点击回调
     */
    ModCardDS(std::vector<ModInfo>& mods, const bool& disabled, std::function<void(size_t)> clickCallback)
        : m_mods(mods), m_disabled(disabled), m_clickCallback(std::move(clickCallback)) {}

    /** @brief 返回 mod 总数 */
    size_t getItemCount() override { return m_mods.size(); }

    /**
     * @brief 取一个空卡片，填上第 index 个 mod 的数据
     * @param grid 网格容器
     * @param index mod 索引
     */
    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto* card = static_cast<ModCard*>(grid->dequeueReusableCell("ModCard"));
        auto& mod = m_mods[index];
        card->setMod(mod.displayName, mod.type, mod.isInstalled, m_disabled, mod.modID, mod.hasUpdate);
        return card;
    }

    /**
     * @brief 用户点击卡片时触发
     * @param grid 网格容器
     * @param index mod 索引
     */
    void onItemSelected(RecyclingGrid* grid, size_t index) override {
        if (m_clickCallback) m_clickCallback(index);
    }

    /** @brief 清空数据（空实现） */
    void clearData() override {}

private:
    std::vector<ModInfo>& m_mods;                    // mod 列表引用（与 ModList 共享）
    const bool& m_disabled;                          // 游戏模组禁用状态引用
    std::function<void(size_t)> m_clickCallback;     // 卡片点击回调
};
