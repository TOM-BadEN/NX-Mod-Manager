/**
 * CommentListDS - 留言列表数据源
 * 将 Comment 数据绑定到 RecyclingGrid 的 CommentCard Cell
 */

#pragma once

#include "view/recyclingGrid.hpp"
#include "view/commentCard.hpp"
#include "api/comment.hpp"
#include <vector>

class CommentListDS : public RecyclingGridDataSource {
public:
    /**
     * @brief 构造留言列表数据源
     * @param comments 留言列表引用
     */
    CommentListDS(std::vector<api::comment::Comment>& comments)
        : m_comments(comments) {}

    /** @brief 返回留言总数 */
    size_t getItemCount() override { return m_comments.size(); }

    /**
     * @brief 取一个空卡片，填上第 index 个留言的数据
     * @param grid 网格容器
     * @param index 留言索引
     */
    RecyclingGridItem* cellForRow(RecyclingGrid* grid, size_t index) override {
        auto* card = static_cast<CommentCard*>(grid->dequeueReusableCell("CommentCard"));
        auto& comment = m_comments[index];
        card->setComment(comment.nickname, comment.content, comment.createTime);
        return card;
    }

    /** @brief 用户点击回调（空实现） */
    void onItemSelected(RecyclingGrid* grid, size_t index) override {}

    /** @brief 清空数据（空实现） */
    void clearData() override {}

private:
    std::vector<api::comment::Comment>& m_comments;     // 留言列表引用（与 Manager 共享）
};
