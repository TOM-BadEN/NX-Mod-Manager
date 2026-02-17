/**
 * GridPage - 通用网格翻页组件
 * 参数化行列数 + 工厂函数动态创建元素，支持 LB/RB 翻页
 * 零业务依赖，数据绑定通过回调实现
 */

#include "view/gridPage.hpp"

// 构造函数：加载布局 + 注册翻页按键
GridPage::GridPage() {
    inflateFromXMLRes("xml/view/gridPage.xml");

    registerAction("上一页", brls::BUTTON_LB, [this](...) {
        prevPage();
        return true;
    }, true, true);

    registerAction("下一页", brls::BUTTON_RB, [this](...) {
        nextPage();
        return true;
    }, true, true);
}

// 初始化网格：动态创建行容器 + 元素
void GridPage::setGrid(int rows, int cols, std::function<brls::View*()> factory) {
    m_rows = rows;
    m_cols = cols;
    m_itemsPerPage = rows * cols;
    m_items.resize(m_itemsPerPage);

    for (int r = 0; r < rows; r++) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setGrow(1);
        row->setJustifyContent(brls::JustifyContent::FLEX_START);
        row->setAlignItems(brls::AlignItems::CENTER);
        addView(row);

        for (int c = 0; c < cols; c++) {
            int i = r * cols + c;
            m_items[i] = factory();
            m_items[i]->setWidth(0);
            m_items[i]->setGrow(1);
            m_items[i]->setShrink(1);
            m_items[i]->setMargins(9, 9, 9, 9);
            row->addView(m_items[i]);

            m_items[i]->getFocusEvent()->subscribe([this, i](brls::View*) {
                if (m_totalItems > 0) m_indexUpdate.update(m_currentPage * m_itemsPerPage + i, m_totalItems);
            });

            m_items[i]->registerClickAction([this, i](brls::View*) {
                int globalIndex = m_currentPage * m_itemsPerPage + i;
                if (m_clickCallback && isItemVisible(i)) m_clickCallback(globalIndex);
                return true;
            });

            m_items[i]->addGestureRecognizer(new brls::TapGestureRecognizer(m_items[i]));
        }
    }
}

// 设置数据（合并 setItemCount + setBindCallback + reloadData）
void GridPage::setData(int count, std::function<void(brls::View*, int)> bind) {
    m_totalItems = count;
    m_bindCallback = bind;
    int maxPage = getTotalPages() - 1;
    if (m_currentPage > maxPage) m_currentPage = maxPage;
    refreshPage();
}

// 设置数据总数
void GridPage::setItemCount(int count) {
    m_totalItems = count;
    int maxPage = getTotalPages() - 1;
    if (m_currentPage > maxPage) m_currentPage = maxPage;
}

// 下一页
void GridPage::nextPage() {
    if (m_currentPage < getTotalPages() - 1) {
        flipPage(1);
        fixFocusAfterFlip();
    } else {
        int focused = findFocusedIndex();
        for (int i = m_itemsPerPage - 1; i > focused; i--) {
            if (isItemVisible(i)) {
                focusItem(i);
                return;
            }
        }
        auto* focus = brls::Application::getCurrentFocus();
        if (focus) focus->shakeHighlight(brls::FocusDirection::RIGHT);
    }
}

// 上一页
void GridPage::prevPage() {
    if (m_currentPage > 0) {
        flipPage(-1);
        fixFocusAfterFlip();
    } else {
        int focused = findFocusedIndex();
        if (focused > 0 && isItemVisible(0)) {
            focusItem(0);
            return;
        }
        auto* focus = brls::Application::getCurrentFocus();
        if (focus) focus->shakeHighlight(brls::FocusDirection::LEFT);
    }
}

int GridPage::getTotalPages() {
    if (m_totalItems <= 0 || m_itemsPerPage <= 0) return 1;
    return (m_totalItems + m_itemsPerPage - 1) / m_itemsPerPage;
}

void GridPage::setIndexChangeCallback(std::function<void(int, int)> callback) {
    m_indexUpdate.setCallback(callback);
}

void GridPage::setClickCallback(std::function<void(int)> callback) {
    m_clickCallback = callback;
}

// 刷新当前页元素（只刷数据，不处理焦点）
void GridPage::refreshPage() {
    if (!m_bindCallback) return;
    int startIndex = m_currentPage * m_itemsPerPage;
    for (int i = 0; i < m_itemsPerPage; i++) {
        int dataIndex = startIndex + i;
        if (dataIndex < m_totalItems) {
            m_items[i]->setVisibility(brls::Visibility::VISIBLE);
            m_bindCallback(m_items[i], dataIndex);
        } else {
            m_items[i]->setVisibility(brls::Visibility::INVISIBLE);
        }
    }
}

// 纯翻页：改页码 + 刷新数据
void GridPage::flipPage(int delta) {
    m_currentPage += delta;
    refreshPage();
}

// 焦点转移 + 索引更新
void GridPage::focusItem(int itemIndex) {
    brls::Application::giveFocus(m_items[itemIndex]);
    m_indexUpdate.update(m_currentPage * m_itemsPerPage + itemIndex, m_totalItems);
}

// 翻页后修正不可见焦点
void GridPage::fixFocusAfterFlip() {
    int focused = findFocusedIndex();
    if (focused >= 0 && !isItemVisible(focused)) focused = findVisibleItem(focused - 1, -1);
    if (focused >= 0) focusItem(focused);
}

// 查找当前聚焦的元素索引，沿 parent 链向上匹配，未找到返回 -1
int GridPage::findFocusedIndex() {
    auto* v = brls::Application::getCurrentFocus();
    while (v) {
        for (int i = 0; i < m_itemsPerPage; i++) {
            if (v == m_items[i]) return i;
        }
        v = v->getParent();
    }
    return -1;
}

// 元素是否可见
bool GridPage::isItemVisible(int index) {
    if (index < 0 || index >= m_itemsPerPage) return false;
    return m_items[index]->getVisibility() == brls::Visibility::VISIBLE;
}

// 从 start 按 step 方向查找最近的可见元素，未找到返回 -1
int GridPage::findVisibleItem(int start, int step) {
    for (int i = start; i >= 0 && i < m_itemsPerPage; i += step) {
        if (isItemVisible(i)) return i;
    }
    return -1;
}

// 焦点导航
brls::View* GridPage::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    int itemIndex = findFocusedIndex();
    if (itemIndex == -1) return brls::Box::getNextFocus(direction, currentView);

    int row = itemIndex / m_cols;
    int col = itemIndex % m_cols;
    int targetIndex = -1;

    switch (direction) {
        case brls::FocusDirection::RIGHT:
            if (col < m_cols - 1 && isItemVisible(itemIndex + 1)) targetIndex = itemIndex + 1;
            else if (row < m_rows - 1 && isItemVisible((row + 1) * m_cols)) targetIndex = (row + 1) * m_cols;
            else if (m_currentPage < getTotalPages() - 1) {
                flipPage(1);
                targetIndex = 0;
            }
            break;

        case brls::FocusDirection::LEFT:
            if (col > 0) targetIndex = itemIndex - 1;
            else if (row > 0) targetIndex = (row - 1) * m_cols + (m_cols - 1);
            else if (m_currentPage > 0) {
                flipPage(-1);
                targetIndex = m_itemsPerPage - 1;
            }
            break;

        case brls::FocusDirection::DOWN:
            if (row < m_rows - 1 && isItemVisible(itemIndex + m_cols)) targetIndex = itemIndex + m_cols;
            else if (m_currentPage < getTotalPages() - 1) {
                flipPage(1);
                targetIndex = col;
                if (!isItemVisible(targetIndex)) targetIndex = findVisibleItem(targetIndex - 1, -1);
            } else {
                targetIndex = findVisibleItem(itemIndex + 1, 1);
            }
            break;

        case brls::FocusDirection::UP:
            if (row > 0) targetIndex = itemIndex - m_cols;
            else if (m_currentPage > 0) {
                flipPage(-1);
                targetIndex = (m_rows - 1) * m_cols + col;
            } else {
                targetIndex = findVisibleItem(itemIndex - 1, -1);
            }
            break;
    }

    if (targetIndex >= 0 && isItemVisible(targetIndex)) {
        m_indexUpdate.update(m_currentPage * m_itemsPerPage + targetIndex, m_totalItems);
        return m_items[targetIndex];
    }

    return nullptr;
}

int GridPage::getCurrentPage() const {
    return m_currentPage;
}

// 更新指定全局索引的槽位
void GridPage::updateSlot(int globalIndex) {
    int itemIndex = globalIndex - m_currentPage * m_itemsPerPage;
    if (itemIndex < 0 || itemIndex >= m_itemsPerPage) return;
    if (m_bindCallback) m_bindCallback(m_items[itemIndex], globalIndex);
}

// 数据变更后重刷当前页（不重置页码）
void GridPage::reloadData() {
    refreshPage();
}

// 工厂函数：用于 XML 注册
brls::View* GridPage::create() {
    return new GridPage();
}
