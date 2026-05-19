/**
 * CommentCard - 留言卡片组件
 */

#include "view/commentCard.hpp"
#include "utils/format.hpp"

CommentCard::CommentCard() {
    inflateFromXMLRes("xml/view/commentCard.xml");
}

void CommentCard::setComment(const std::string& nickname, const std::string& content, const std::string& createTime) {
    m_nickname->setText(nickname);
    m_content->setText(content);
    m_time->setText(format::timeAgo(createTime));
}

void CommentCard::prepareForReuse() {
    m_nickname->setText("");
    m_content->setText("");
    m_time->setText("");
}

RecyclingGridItem* CommentCard::create() {
    return new CommentCard();
}
