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
    ModCardDS(std::vector<ModInfo>& mods, std::function<void(size_t)> clickCallback)
        : m_mods(mods), m_clickCallback(std::move(clickCallback)) {}

    // 返回 mod 总数
    size_t getItemCount() override { return m_mods.size(); }

    // 取一个空卡片，填上第 index 个 mod 的数据，交给网格显示
    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto* card = static_cast<ModCard*>(grid->dequeueReusableCell("ModCard"));
        auto& mod = m_mods[index];
        card->setMod(mod.displayName, mod.type, mod.isInstalled);
        return card;
    }

    // 用户点击第 index 个卡片时触发
    void onItemSelected(RecyclingGrid* grid, size_t index) override {
        if (m_clickCallback) m_clickCallback(index);
    }

    // 清空数据（Mod 列表不会动态清空，空实现）
    void clearData() override {}

private:
    std::vector<ModInfo>& m_mods;                    // mod 列表引用（与 ModManager 共享）
    std::function<void(size_t)> m_clickCallback;     // 卡片点击回调
};
