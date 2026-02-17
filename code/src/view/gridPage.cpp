/**
 * GridPage - 通用滚动网格组件
 * 继承 ScrollingFrame（CENTERED），支持触屏滑动 + 方向键滚动 + LB/RB 快速翻屏
 * 导航采用 index±cols 数学保列，零业务依赖，数据绑定通过回调实现
 */

#include "view/gridPage.hpp"

// ── GridContentBox ──────────────────────────────────────────────

GridContentBox::GridContentBox(GridPage* grid) : Box(brls::Axis::COLUMN), m_grid(grid) {}

brls::View* GridContentBox::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    return m_grid->getNextCellFocus(direction, currentView);
}

// ── GridPage ────────────────────────────────────────────────────

GridPage::GridPage() {
    setScrollingBehavior(brls::ScrollingBehavior::CENTERED);

    m_contentBox = new GridContentBox(this);
    m_contentBox->setGrow(1);
    m_contentBox->setPadding(10, 40, 10, 40);
    setContentView(m_contentBox);

    registerAction("上翻", brls::BUTTON_LB, [this](...) {
        flipScreen(-1);
        return true;
    }, true, true);

    registerAction("下翻", brls::BUTTON_RB, [this](...) {
        flipScreen(1);
        return true;
    }, true, true);
}

void GridPage::setGrid(int cols, std::function<brls::View*()> factory) {
    m_cols = cols;
    m_factory = factory;
}

void GridPage::setData(int count, std::function<void(brls::View*, int)> bind) {
    m_totalItems = count;
    m_bindCallback = bind;
    rebuild();
    bindAll();
    if (m_totalItems > 0) m_indexUpdate.update(0, m_totalItems);
}

void GridPage::setItemCount(int count) {
    if (count == m_totalItems) return;
    int savedIndex = findFocusedIndex();
    m_totalItems = count;
    rebuild();
    bindAll();
    if (m_totalItems > 0) {
        int restoreIndex = std::clamp(savedIndex, 0, m_totalItems - 1);
        brls::Application::giveFocus(m_items[restoreIndex]);
    }
}

void GridPage::reloadData() {
    bindAll();
}

void GridPage::updateSlot(int globalIndex) {
    if (globalIndex < 0 || globalIndex >= static_cast<int>(m_items.size())) return;
    if (m_bindCallback) m_bindCallback(m_items[globalIndex], globalIndex);
}

void GridPage::setClickCallback(std::function<void(int)> callback) {
    m_clickCallback = callback;
}

void GridPage::setIndexChangeCallback(std::function<void(int, int)> callback) {
    m_indexUpdate.setCallback(callback);
    m_indexUpdate.notify();
}

// 导航：index±cols 保列，index±1 左右移动
brls::View* GridPage::getNextCellFocus(brls::FocusDirection direction, brls::View* currentView) {
    int idx = findFocusedIndex();
    if (idx < 0) return Box::getNextFocus(direction, currentView);

    int target = -1;
    switch (direction) {
        case brls::FocusDirection::DOWN:
            target = idx + m_cols;
            if (target >= m_totalItems) target = -1;
            break;
        case brls::FocusDirection::UP:
            target = idx - m_cols;
            if (target < 0) target = -1;
            break;
        case brls::FocusDirection::RIGHT:
            if ((idx + 1) % m_cols != 0 && idx + 1 < m_totalItems) target = idx + 1;
            break;
        case brls::FocusDirection::LEFT:
            if (idx % m_cols != 0) target = idx - 1;
            break;
    }

    if (target >= 0 && target < m_totalItems) return m_items[target];

    // 边界：交给父级处理（退出网格）
    brls::View* next = getParentNavigationDecision(this, nullptr, direction);
    if (!next && hasParent()) next = getParent()->getNextFocus(direction, this);
    return next;
}

// LB/RB 翻屏：临时将动画时长设为 1ms，走框架正常的 giveFocus 路径
// giveFocus 内部触发 onFocusLost(旧高亮渐隐1ms) + onFocusGained(新高亮渐入1ms) + updateScrolling(滚动动画1ms)
// updateTickings 的 16ms delta 远大于 1ms，三个动画在渲染前全部完成，消除高亮逃逸闪烁
void GridPage::flipScreen(int direction) {
    int idx = findFocusedIndex();
    if (idx < 0 || m_items.empty()) return;
    auto& rows = m_contentBox->getChildren();
    if (rows.empty()) return;

    float rowH = rows[0]->getHeight();
    int rowsPerScreen = (rowH > 0) ? std::max(1, static_cast<int>(getHeight() / rowH)) : 3;
    int target = idx + direction * m_cols * rowsPerScreen;
    target = std::clamp(target, 0, m_totalItems - 1);
    if (target == idx) return;

    auto style = brls::Application::getStyle();
    float saved = style["brls/animations/highlight"];
    style.addMetric("brls/animations/highlight", 1.0f);

    brls::Application::giveFocus(m_items[target]);

    style.addMetric("brls/animations/highlight", saved);
}

// 查找当前聚焦元素的全局索引，沿 parent 链匹配 m_items
int GridPage::findFocusedIndex() {
    auto* v = brls::Application::getCurrentFocus();
    while (v) {
        for (int i = 0; i < static_cast<int>(m_items.size()); i++) {
            if (v == m_items[i]) return i;
        }
        v = v->getParent();
    }
    return -1;
}

void GridPage::rebuild() {
    if (!m_factory || m_cols <= 0) return;

    m_contentBox->clearViews();
    m_items.clear();

    int totalRows = (m_totalItems + m_cols - 1) / m_cols;

    for (int r = 0; r < totalRows; r++) {
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setJustifyContent(brls::JustifyContent::FLEX_START);
        row->setAlignItems(brls::AlignItems::CENTER);
        m_contentBox->addView(row);

        int itemsInRow = std::min(m_cols, m_totalItems - r * m_cols);

        for (int c = 0; c < itemsInRow; c++) {
            int i = r * m_cols + c;
            auto* item = m_factory();
            item->setWidth(0);
            item->setGrow(1);
            item->setShrink(1);
            item->setMargins(9, 9, 9, 9);
            row->addView(item);
            m_items.push_back(item);

            item->getFocusEvent()->subscribe([this, i](brls::View*) {
                if (m_totalItems > 0) m_indexUpdate.update(i, m_totalItems);
            });

            item->registerClickAction([this, i](brls::View*) {
                if (m_clickCallback) m_clickCallback(i);
                return true;
            });

            item->addGestureRecognizer(new brls::TapGestureRecognizer(item));
        }

        // 最后一行不满时补不可见占位，保持均分布局
        for (int c = itemsInRow; c < m_cols; c++) {
            auto* placeholder = m_factory();
            placeholder->setVisibility(brls::Visibility::INVISIBLE);
            placeholder->setWidth(0);
            placeholder->setGrow(1);
            placeholder->setShrink(1);
            placeholder->setMargins(9, 9, 9, 9);
            row->addView(placeholder);
        }
    }
}

void GridPage::bindAll() {
    if (!m_bindCallback) return;
    for (int i = 0; i < static_cast<int>(m_items.size()); i++) {
        m_bindCallback(m_items[i], i);
    }
}

brls::View* GridPage::create() {
    return new GridPage();
}
