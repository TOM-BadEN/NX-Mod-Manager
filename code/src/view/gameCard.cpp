/**
 * GameCard - 游戏卡片组件
 * 显示单个游戏的图标、名称、版本和 mod 数量
 */

#include "view/gameCard.hpp"

// 构造函数：加载 XML 布局
GameCard::GameCard() {
    inflateFromXMLRes("xml/view/gameCard.xml");
}

// 设置卡片数据
void GameCard::setGame(const std::string& name, const std::string& version, const std::string& modCount) {
    m_name->setText(name);
    m_version->setText("版本：" + version);
    m_modCount->setText("数量：" + modCount);
    setVisibility(brls::Visibility::VISIBLE);
    setFocusable(true);
}

// 设置游戏图标（传入 NVG image ID）
void GameCard::setIcon(int imageId) {
    if (imageId > 0) m_icon->innerSetImage(imageId);
}                                 

// 清空卡片（隐藏，图标恢复默认）
void GameCard::clear() {
    m_name->setText("");
    m_version->setText("");
    m_modCount->setText("");
    m_icon->setImageFromRes("img/defaultIcon.jpg");
    setVisibility(brls::Visibility::INVISIBLE);
    setFocusable(false);
}

// 工厂函数：用于 XML 注册
brls::View* GameCard::create() {
    return new GameCard();
}
