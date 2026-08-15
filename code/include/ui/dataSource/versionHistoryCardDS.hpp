/**
 * VersionHistoryCardDS - 历史版本卡片数据源
 * 将版本号绑定到 RecyclingGrid 的 VersionHistoryCard Cell
 */

#pragma once

#include "ui/view/recyclingGrid.hpp"
#include "ui/view/versionHistoryCard.hpp"
#include <string>
#include <vector>

class VersionHistoryCardDS : public RecyclingGridDataSource {
public:
    /**
     * @brief 构造历史版本卡片数据源
     * @param versionTags 版本号列表引用
     */
    VersionHistoryCardDS(std::vector<std::string>& versionTags) : m_versionTags(versionTags) {}

    /**
     * @brief 返回历史版本总数
     * @return 历史版本总数
     */
    size_t getItemCount() override { return m_versionTags.size(); }

    /**
     * @brief 取一个空卡片，填上第 index 个版本号
     * @param grid 网格容器
     * @param index 版本索引
     * @return 已绑定版本号的卡片
     */
    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto* card = static_cast<VersionHistoryCard*>(grid->dequeueReusableCell("VersionHistoryCard"));
        card->setTitle(m_versionTags[index]);
        return card;
    }

    /** @brief 清空数据（空实现） */
    void clearData() override {}

private:
    std::vector<std::string>& m_versionTags; // 版本号列表引用
};
