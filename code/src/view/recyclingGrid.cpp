/**
 * RecyclingGrid - 通用回收网格组件
 * 移植自 wiliwili 项目，适配本项目
 */

#include "view/recyclingGrid.hpp"

// ── RecyclingGridItem ──────────────────────────────────────────

RecyclingGridItem::RecyclingGridItem() {
    setFocusable(true);
    registerClickAction([this](View* view) {
        auto* recycler = dynamic_cast<RecyclingGrid*>(getParent()->getParent());
        if (recycler) recycler->getDataSource()->onItemSelected(recycler, index);
        return true;
    });
    addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

size_t RecyclingGridItem::getIndex() const { return index; }

void RecyclingGridItem::setIndex(size_t value) { index = value; }

RecyclingGridItem::~RecyclingGridItem() = default;

// ── RecyclingGridContentBox ────────────────────────────────────

RecyclingGridContentBox::RecyclingGridContentBox(RecyclingGrid* recycler)
    : Box(brls::Axis::ROW), m_recycler(recycler) {}

brls::View* RecyclingGridContentBox::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    return m_recycler->getNextCellFocus(direction, currentView);
}

// ── RecyclingGrid ──────────────────────────────────────────────

RecyclingGrid::RecyclingGrid() {
    brls::Logger::debug("View RecyclingGrid: create");

    setFocusable(false);
    setScrollingBehavior(brls::ScrollingBehavior::CENTERED);

    m_contentBox = new RecyclingGridContentBox(this);
    setContentView(m_contentBox);

    registerFloatXMLAttribute("itemHeight", [this](float value) {
        estimatedRowHeight = value;
        reloadData();
    });

    registerFloatXMLAttribute("spanCount", [this](float value) {
        if (value != 1) isFlowMode = false;
        spanCount = value;
        reloadData();
    });

    registerFloatXMLAttribute("itemSpace", [this](float value) {
        estimatedRowSpace = value;
        reloadData();
    });

    registerFloatXMLAttribute("preFetchLine", [this](float value) {
        preFetchLine = value;
        reloadData();
    });

    registerBoolXMLAttribute("flowMode", [this](bool value) {
        spanCount  = 1;
        isFlowMode = value;
        reloadData();
    });

    registerPercentageXMLAttribute("paddingRight",
                                     [this](float percentage) { setPaddingRightPercentage(percentage); });

    registerPercentageXMLAttribute("paddingLeft",
                                     [this](float percentage) { setPaddingLeftPercentage(percentage); });
}

RecyclingGrid::~RecyclingGrid() {
    brls::Logger::debug("View RecyclingGrid: delete");
    delete m_dataSource;
    for (const auto& it : m_queueMap) {
        for (auto item : *it.second) {
            item->setParent(nullptr);
            if (item->isPtrLocked())
                item->freeView();
            else
                delete item;
        }
        delete it.second;
    }
}

// ── draw ───────────────────────────────────────────────────────

void RecyclingGrid::draw(NVGcontext* vg, float x, float y, float width, float height,
                         brls::Style style, brls::FrameContext* ctx) {
    itemsRecyclingLoop();
    ScrollingFrame::draw(vg, x, y, width, height, style, ctx);
}

// ── Cell 注册与获取 ────────────────────────────────────────────

void RecyclingGrid::registerCell(std::string identifier, std::function<RecyclingGridItem*()> allocation) {
    m_queueMap.insert(std::make_pair(identifier, new std::vector<RecyclingGridItem*>()));
    m_allocationMap.insert(std::make_pair(identifier, allocation));
}

RecyclingGridItem* RecyclingGrid::dequeueReusableCell(std::string identifier) {
    brls::Logger::verbose("RecyclingGrid::dequeueReusableCell: {}", identifier);
    RecyclingGridItem* cell = nullptr;
    auto it = m_queueMap.find(identifier);

    if (it != m_queueMap.end()) {
        std::vector<RecyclingGridItem*>* vector = it->second;
        if (!vector->empty()) {
            cell = vector->back();
            vector->pop_back();
        } else {
            cell                  = m_allocationMap.at(identifier)();
            cell->reuseIdentifier = identifier;
            cell->detach();
        }
    }

    if (cell) {
        cell->setHeight(brls::View::AUTO);
        cell->prepareForReuse();
    }

    return cell;
}

// ── 数据源 ─────────────────────────────────────────────────────

void RecyclingGrid::setDataSource(RecyclingGridDataSource* source) {
    if (m_dataSource) delete m_dataSource;

    m_requestNextPage = false;
    m_dataSource      = source;
    if (m_layouted) reloadData();
}

RecyclingGridDataSource* RecyclingGrid::getDataSource() const { return m_dataSource; }

void RecyclingGrid::reloadData() {
    if (!m_layouted) return;

    // 将所有节点回收
    auto& children = m_contentBox->getChildren();
    for (auto const& child : children) {
        queueReusableCell((RecyclingGridItem*)child);
        child->willDisappear(true);
    }
    children.clear();

    visibleMin = UINT_MAX;
    visibleMax = 0;

    m_renderedFrame            = brls::Rect();
    m_renderedFrame.size.width = getWidth();
    if (m_renderedFrame.size.width != m_renderedFrame.size.width) {
        // width 为 NAN 时使用历史宽度
        m_renderedFrame.size.width = m_oldWidth;
    }

    setContentOffsetY(0, false);
    if (!m_dataSource) return;
    if (m_dataSource->getItemCount() <= 0) {
        m_contentBox->setHeight(0);
        return;
    }
    size_t cellFocusIndex = m_defaultCellFocus;
    if (cellFocusIndex >= m_dataSource->getItemCount()) cellFocusIndex = m_dataSource->getItemCount() - 1;

    // 设置列表总高度
    if (!isFlowMode || spanCount != 1) {
        m_contentBox->setHeight((estimatedRowHeight + estimatedRowSpace) * (float)getRowCount() +
                                m_paddingTop + m_paddingBottom);
        size_t lineHeadIndex = cellFocusIndex / spanCount * spanCount;
        m_renderedFrame.origin.y = getHeightByCellIndex(lineHeadIndex);
        addCellAt(lineHeadIndex, true);
    } else {
        m_cellHeightCache.clear();
        for (size_t section = 0; section < m_dataSource->getItemCount(); section++) {
            float height = m_dataSource->heightForRow(this, section);
            m_cellHeightCache.push_back(height);
        }
        m_contentBox->setHeight(getHeightByCellIndex(m_dataSource->getItemCount()) + m_paddingTop + m_paddingBottom);
        addCellAt(0, true);
    }

    selectRowAt(cellFocusIndex, false);
}

void RecyclingGrid::notifyDataChanged() {
    if (!m_layouted) return;

    if (m_dataSource) {
        if (isFlowMode) {
            for (size_t i = m_cellHeightCache.size(); i < m_dataSource->getItemCount(); i++) {
                float height = m_dataSource->heightForRow(this, i);
                m_cellHeightCache.push_back(height);
            }
            m_contentBox->setHeight(getHeightByCellIndex(m_dataSource->getItemCount()) + m_paddingTop + m_paddingBottom);
        } else {
            m_contentBox->setHeight((estimatedRowHeight + estimatedRowSpace) * getRowCount() +
                                    m_paddingTop + m_paddingBottom);
        }
    }
    m_requestNextPage = false;
}

void RecyclingGrid::clearData() {
    if (m_dataSource) {
        m_dataSource->clearData();
        reloadData();
    }
}

// ── 查询 ───────────────────────────────────────────────────────

RecyclingGridItem* RecyclingGrid::getGridItemByIndex(size_t index) {
    for (brls::View* i : m_contentBox->getChildren()) {
        auto* v = dynamic_cast<RecyclingGridItem*>(i);
        if (!v) continue;
        if (v->getIndex() == index) return v;
    }
    return nullptr;
}

std::vector<RecyclingGridItem*>& RecyclingGrid::getGridItems() {
    return (std::vector<RecyclingGridItem*>&)m_contentBox->getChildren();
}

size_t RecyclingGrid::getItemCount() { return m_dataSource->getItemCount(); }

size_t RecyclingGrid::getRowCount() { return (m_dataSource->getItemCount() - 1) / spanCount + 1; }

float RecyclingGrid::getHeightByCellIndex(size_t index, size_t start) {
    if (index <= start) return 0;
    if (!isFlowMode) return (estimatedRowHeight + estimatedRowSpace) * (size_t)((index - start) / spanCount);

    if (m_cellHeightCache.empty()) {
        brls::Logger::error("cellHeightCache.size() cannot be zero in flow mode {} {}", start, index);
        return 0;
    }

    if (index > m_cellHeightCache.size()) index = m_cellHeightCache.size();

    float res = 0;
    for (size_t i = start; i < index && i < m_cellHeightCache.size(); i++) {
        if (m_cellHeightCache[i] != -1)
            res += m_cellHeightCache[i] + estimatedRowSpace;
        else
            res += estimatedRowHeight + estimatedRowSpace;
    }
    return res;
}

// ── 焦点 ───────────────────────────────────────────────────────

void RecyclingGrid::setDefaultCellFocus(size_t index) { m_defaultCellFocus = index; }

size_t RecyclingGrid::getDefaultCellFocus() const { return m_defaultCellFocus; }

brls::View* RecyclingGrid::getDefaultFocus() {
    if (m_dataSource && m_dataSource->getItemCount() > 0) return ScrollingFrame::getDefaultFocus();
    return nullptr;
}

void RecyclingGrid::selectRowAt(size_t index, bool animated) {
    setContentOffsetY(getHeightByCellIndex(index), animated);
    itemsRecyclingLoop();

    for (View* view : m_contentBox->getChildren()) {
        if (*((size_t*)view->getParentUserData()) == index) {
            m_contentBox->setLastFocusedView(view);
            break;
        }
    }
}

void RecyclingGrid::setFocusChangeCallback(std::function<void(size_t)> callback) {
    m_focusChangeCallback = std::move(callback);
}

void RecyclingGrid::onChildFocusGained(View* directChild, View* focusedView) {
    ScrollingFrame::onChildFocusGained(directChild, focusedView);
    if (!m_focusChangeCallback) return;
    View* v = focusedView;
    while (v && !dynamic_cast<RecyclingGridItem*>(v)) v = v->getParent();
    if (v) m_focusChangeCallback(static_cast<RecyclingGridItem*>(v)->getIndex());
}

// ── 导航 ───────────────────────────────────────────────────────

brls::View* RecyclingGrid::getNextCellFocus(brls::FocusDirection direction, brls::View* currentView) {
    void* parentUserData = currentView->getParentUserData();

    // 上下导航：index ± spanCount
    if (m_contentBox->getAxis() == brls::Axis::ROW &&
        direction != brls::FocusDirection::LEFT && direction != brls::FocusDirection::RIGHT) {

        int row_offset = spanCount;
        if (direction == brls::FocusDirection::UP) row_offset = -spanCount;
        View* row_currentFocus       = nullptr;
        size_t row_currentFocusIndex = *((size_t*)parentUserData) + row_offset;

        if (row_currentFocusIndex >= m_dataSource->getItemCount()) {
            row_currentFocusIndex -= *((size_t*)parentUserData) % spanCount;
        }

        while (!row_currentFocus && row_currentFocusIndex >= 0 &&
               row_currentFocusIndex < m_dataSource->getItemCount()) {
            for (auto it : m_contentBox->getChildren()) {
                if (*((size_t*)it->getParentUserData()) == row_currentFocusIndex) {
                    row_currentFocus = it->getDefaultFocus();
                    break;
                }
            }
            row_currentFocusIndex += row_offset;
        }
        if (row_currentFocus) {
            itemsRecyclingLoop();
            return row_currentFocus;
        }
    }

    // 左右边界检查
    if (m_contentBox->getAxis() == brls::Axis::ROW) {
        int position = *((size_t*)parentUserData) % spanCount;
        if ((direction == brls::FocusDirection::LEFT && position == 0) ||
            (direction == brls::FocusDirection::RIGHT && position == (spanCount - 1))) {
            View* next = getParentNavigationDecision(this, nullptr, direction);
            if (!next && hasParent()) next = getParent()->getNextFocus(direction, this);
            return next;
        }
    }

    // 方向不匹配时退出
    if ((m_contentBox->getAxis() == brls::Axis::ROW &&
         direction != brls::FocusDirection::LEFT && direction != brls::FocusDirection::RIGHT) ||
        (m_contentBox->getAxis() == brls::Axis::COLUMN &&
         direction != brls::FocusDirection::UP && direction != brls::FocusDirection::DOWN)) {
        View* next = getParentNavigationDecision(this, nullptr, direction);
        if (!next && hasParent()) next = getParent()->getNextFocus(direction, this);
        return next;
    }

    // 左右遍历
    size_t offset = 1;
    if ((m_contentBox->getAxis() == brls::Axis::ROW && direction == brls::FocusDirection::LEFT) ||
        (m_contentBox->getAxis() == brls::Axis::COLUMN && direction == brls::FocusDirection::UP)) {
        offset = -1;
    }

    size_t currentFocusIndex = *((size_t*)parentUserData) + offset;
    View* currentFocus       = nullptr;

    while (!currentFocus && currentFocusIndex >= 0 && currentFocusIndex < m_dataSource->getItemCount()) {
        for (auto it : m_contentBox->getChildren()) {
            if (*((size_t*)it->getParentUserData()) == currentFocusIndex) {
                currentFocus = it->getDefaultFocus();
                break;
            }
        }
        currentFocusIndex += offset;
    }

    currentFocus = getParentNavigationDecision(this, currentFocus, direction);
    if (!currentFocus && hasParent()) currentFocus = getParent()->getNextFocus(direction, this);
    return currentFocus;
}

// ── 分页 ───────────────────────────────────────────────────────

void RecyclingGrid::onNextPage(const std::function<void()>& callback) { m_nextPageCallback = callback; }

void RecyclingGrid::forceRequestNextPage() { m_requestNextPage = false; }

// ── 布局 ───────────────────────────────────────────────────────

void RecyclingGrid::onLayout() {
    ScrollingFrame::onLayout();
    auto rect   = getFrame();
    float width = rect.getWidth();
    // check NAN
    if (width != width) return;

    if (!m_contentBox) return;
    m_contentBox->setWidth(width);
    if (checkWidth()) {
        brls::Logger::debug("RecyclingGrid::onLayout reloadData()");
        m_layouted = true;
        reloadData();
    }
}

bool RecyclingGrid::checkWidth() {
    float width = getWidth();
    if (m_oldWidth == -1) m_oldWidth = width;
    if ((int)m_oldWidth != (int)width && width != 0) {
        brls::Logger::debug("RecyclingGrid::checkWidth from {} to {}", m_oldWidth, width);
        m_oldWidth = width;
        return true;
    }
    m_oldWidth = width;
    return false;
}

// ── Cell 回收循环 ──────────────────────────────────────────────

void RecyclingGrid::addCellAt(size_t index, bool downSide) {
    RecyclingGridItem* cell = m_dataSource->cellForRow(this, index);

    float cellHeight = estimatedRowHeight;
    float cellWidth  = (m_renderedFrame.getWidth() - getPaddingLeft() - getPaddingRight()) / spanCount -
                      cell->getMarginLeft() - cell->getMarginRight();
    float cellX = m_renderedFrame.getMinX() + getPaddingLeft();

    if (isFlowMode) {
        cell->setWidth(cellWidth);
        if (m_cellHeightCache[index] == -1) {
            cellHeight = cell->getHeight();
            if (cellHeight > estimatedRowHeight) cellHeight = estimatedRowHeight;
            m_cellHeightCache[index] = cellHeight;
        } else {
            cellHeight = m_cellHeightCache[index];
        }
        brls::Logger::verbose("Add cell at: y {} height {}", getHeightByCellIndex(index) + m_paddingTop, cellHeight);
    } else {
        cell->setWidth(cellWidth - estimatedRowSpace);
        cellX += (m_renderedFrame.getWidth() - getPaddingLeft() - getPaddingRight()) / spanCount * (index % spanCount);
    }

    cell->setHeight(cellHeight);
    cell->setDetachedPositionX(cellX);
    cell->setDetachedPositionY(getHeightByCellIndex(index) + m_paddingTop);
    cell->setIndex(index);

    m_contentBox->getChildren().insert(m_contentBox->getChildren().end(), cell);

    size_t* userdata = (size_t*)malloc(sizeof(size_t));
    *userdata        = index;
    cell->setParent(m_contentBox, userdata);

    m_contentBox->invalidate();
    cell->View::willAppear();

    if (index < visibleMin) visibleMin = index;
    if (index > visibleMax) visibleMax = index;

    if (index % spanCount == 0) {
        if (!downSide) m_renderedFrame.origin.y -= cellHeight + estimatedRowSpace;
        m_renderedFrame.size.height += cellHeight + estimatedRowSpace;
    }

    if (isFlowMode)
        m_contentBox->setHeight(getHeightByCellIndex(m_dataSource->getItemCount()) + m_paddingTop + m_paddingBottom);

    brls::Logger::verbose("Cell #{} - added", index);
}

void RecyclingGrid::removeCell(brls::View* view) {
    if (!view) return;

    size_t index;
    bool found = false;
    auto& children = m_contentBox->getChildren();

    for (size_t i = 0; i < children.size(); i++) {
        if (children[i] == view) {
            found = true;
            index = i;
            break;
        }
    }

    if (!found) return;

    children.erase(children.begin() + index);
    view->willDisappear(true);
    invalidate();
}

void RecyclingGrid::queueReusableCell(RecyclingGridItem* cell) {
    m_queueMap.at(cell->reuseIdentifier)->push_back(cell);
    cell->cacheForReuse();
}

void RecyclingGrid::itemsRecyclingLoop() {
    if (!m_dataSource) return;

    brls::Rect visibleFrame = getVisibleFrame();

    // 上方元素自动回收
    while (true) {
        RecyclingGridItem* minCell = nullptr;
        for (auto it : m_contentBox->getChildren())
            if (*((size_t*)it->getParentUserData()) == visibleMin) minCell = (RecyclingGridItem*)it;

        if (!minCell || (minCell->getDetachedPosition().y +
                             getHeightByCellIndex(visibleMin + (preFetchLine + 1) * spanCount, visibleMin) >=
                         visibleFrame.getMinY()))
            break;

        float cellHeight = estimatedRowHeight;
        if (isFlowMode) cellHeight = m_cellHeightCache[visibleMin];

        m_renderedFrame.origin.y += minCell->getIndex() % spanCount == 0 ? cellHeight + estimatedRowSpace : 0;
        m_renderedFrame.size.height -= minCell->getIndex() % spanCount == 0 ? cellHeight + estimatedRowSpace : 0;

        queueReusableCell(minCell);
        removeCell(minCell);

        brls::Logger::verbose("Cell #{} - destroyed", visibleMin);
        visibleMin++;
    }

    // 下方元素自动回收
    while (true) {
        RecyclingGridItem* maxCell = nullptr;
        for (auto it : m_contentBox->getChildren())
            if (*((size_t*)it->getParentUserData()) == visibleMax) maxCell = (RecyclingGridItem*)it;

        if (!maxCell || (maxCell->getDetachedPosition().y -
                             getHeightByCellIndex(visibleMax, visibleMax - preFetchLine * spanCount) <=
                         visibleFrame.getMaxY()))
            break;
        if (visibleMax == 0) break;

        float cellHeight = estimatedRowHeight;
        if (isFlowMode) cellHeight = m_cellHeightCache[visibleMax];

        m_renderedFrame.size.height -= maxCell->getIndex() % spanCount == 0 ? cellHeight + estimatedRowSpace : 0;

        queueReusableCell(maxCell);
        removeCell(maxCell);

        brls::Logger::verbose("Cell #{} - destroyed", visibleMax);
        visibleMax--;
    }

    // 上方元素自动添加
    while (visibleMin - 1 < m_dataSource->getItemCount()) {
        if ((visibleMin) % spanCount == 0)
            if (m_renderedFrame.getMinY() + getHeightByCellIndex(visibleMin + preFetchLine * spanCount, visibleMin) <
                visibleFrame.getMinY() - m_paddingTop) {
                break;
            }
        addCellAt(visibleMin - 1, false);
    }

    // 下方元素自动添加
    while (visibleMax + 1 < m_dataSource->getItemCount()) {
        if ((visibleMax + 1) % spanCount == 0)
            if (m_renderedFrame.getMaxY() -
                    getHeightByCellIndex(visibleMax + 1, visibleMax + 1 - preFetchLine * spanCount) >
                visibleFrame.getMaxY() - m_paddingBottom) {
                m_requestNextPage = false;
                break;
            }
        addCellAt(visibleMax + 1, true);
    }

    if (visibleMax + 1 >= getItemCount()) {
        if (!m_requestNextPage && m_nextPageCallback) {
            if (m_dataSource && m_dataSource->getItemCount() > 0) {
                brls::Logger::debug("RecyclingGrid request next page");
                m_requestNextPage = true;
                m_nextPageCallback();
            }
        }
    }
}

// ── Padding ────────────────────────────────────────────────────

void RecyclingGrid::setPadding(float padding) { setPadding(padding, padding, padding, padding); }

void RecyclingGrid::setPadding(float top, float right, float bottom, float left) {
    m_paddingPercentage = false;
    m_paddingTop        = top;
    m_paddingRight      = right;
    m_paddingBottom     = bottom;
    m_paddingLeft       = left;
    reloadData();
}

void RecyclingGrid::setPaddingTop(float top) { m_paddingTop = top; reloadData(); }

void RecyclingGrid::setPaddingRight(float right) {
    m_paddingPercentage = false;
    m_paddingRight      = right;
    reloadData();
}

void RecyclingGrid::setPaddingBottom(float bottom) { m_paddingBottom = bottom; reloadData(); }

void RecyclingGrid::setPaddingLeft(float left) {
    m_paddingPercentage = false;
    m_paddingLeft       = left;
    reloadData();
}

void RecyclingGrid::setPaddingRightPercentage(float right) {
    m_paddingPercentage = true;
    m_paddingRight      = right / 100.0f;
}

void RecyclingGrid::setPaddingLeftPercentage(float left) {
    m_paddingPercentage = true;
    m_paddingLeft       = left / 100.0f;
}

float RecyclingGrid::getPaddingLeft() {
    return m_paddingPercentage ? m_renderedFrame.getWidth() * m_paddingLeft : m_paddingLeft;
}

float RecyclingGrid::getPaddingRight() {
    return m_paddingPercentage ? m_renderedFrame.getWidth() * m_paddingRight : m_paddingRight;
}

// ── 工厂 ───────────────────────────────────────────────────────

brls::View* RecyclingGrid::create() { return new RecyclingGrid(); }
