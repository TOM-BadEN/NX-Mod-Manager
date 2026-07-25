/**
 * CommentCard - 留言卡片组件
 * 显示单条留言：昵称和时间同行两端对齐，留言内容多行自动换行
 */

#pragma once

#include "ui/view/recyclingGrid.hpp"
#include <borealis.hpp>

class CommentCard : public RecyclingGridItem {
public:
    CommentCard();

    /**
     * @brief 设置卡片数据
     * @param nickname 昵称
     * @param content 留言内容
     * @param createTime 创建时间
     */
    void setComment(const std::string& nickname, const std::string& content, const std::string& createTime);

    /** @brief 回收时重置状态 */
    void prepareForReuse() override;

    /** @brief 工厂函数 */
    static RecyclingGridItem* create();

private:
    BRLS_BIND(brls::Label, m_nickname, "commentCard/nickname"); // 昵称
    BRLS_BIND(brls::Label, m_content, "commentCard/content");   // 留言内容
    BRLS_BIND(brls::Label, m_time, "commentCard/time");         // 创建时间
};
