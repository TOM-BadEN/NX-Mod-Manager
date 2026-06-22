/**
 * HelpCardDS - 帮助卡片数据源
 * 将 HelpEntry 数据绑定到 RecyclingGrid 的 HelpCard Cell
 */

#pragma once

#include "view/recyclingGrid.hpp"
#include "view/helpCard.hpp"
#include <vector>
#include <string>

class HelpCardDS : public RecyclingGridDataSource {
public:
    /**
     * @brief 构造帮助卡片数据源
     * @param titles 标题列表引用
     */
    HelpCardDS(std::vector<std::string>& titles) : m_titles(titles) {}

    /** @brief 返回条目总数 */
    size_t getItemCount() override { return m_titles.size(); }

    /**
     * @brief 取一个空卡片，填上第 index 个条目的标题
     * @param grid 网格容器
     * @param index 条目索引
     */
    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto* card = static_cast<HelpCard*>(grid->dequeueReusableCell("HelpCard"));
        card->setTitle(m_titles[index]);
        return card;
    }

    /** @brief 清空数据（空实现） */
    void clearData() override {}

private:
    std::vector<std::string>& m_titles;
};
