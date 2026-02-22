/**
 * RecyclingGrid - 通用回收网格组件
 * 继承 ScrollingFrame，支持多列网格布局 + Cell 回收复用 + 分页加载
 * 移植自 wiliwili 项目，移除了 ButtonRefresh / SkeletonCell 等特有依赖
 * https://github.com/xfangfang/wiliwili
 */

#pragma once

#include <map>
#include <functional>
#include <borealis.hpp>

class RecyclingGrid;

// ── Cell 基类 ──────────────────────────────────────────────────

class RecyclingGridItem : public brls::Box {
public:
    RecyclingGridItem();
    ~RecyclingGridItem() override;

    size_t getIndex() const;
    void setIndex(size_t value);

    std::string reuseIdentifier;

    virtual void prepareForReuse() {}
    virtual void cacheForReuse() {}

private:
    size_t index = 0;
};

// ── 数据源接口 ─────────────────────────────────────────────────

class RecyclingGridDataSource {
public:
    virtual ~RecyclingGridDataSource() = default;

    virtual size_t getItemCount() { return 0; }
    virtual RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) { return nullptr; }
    virtual float heightForRow(RecyclingGrid* recycler, size_t index) { return -1; }
    virtual void onItemSelected(RecyclingGrid* recycler, size_t index) {}
    virtual void clearData() = 0;
};

// ── 内容容器 ───────────────────────────────────────────────────

class RecyclingGridContentBox : public brls::Box {
public:
    RecyclingGridContentBox(RecyclingGrid* recycler);
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

private:
    RecyclingGrid* m_recycler;
};

// ── 核心组件 ───────────────────────────────────────────────────

class RecyclingGrid : public brls::ScrollingFrame {
public:
    RecyclingGrid();
    ~RecyclingGrid() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height,
              brls::Style style, brls::FrameContext* ctx) override;
    void onLayout() override;
    void onChildFocusGained(View* directChild, View* focusedView) override;
    brls::View* getDefaultFocus() override;

    // Cell 注册与获取
    void registerCell(std::string identifier, std::function<RecyclingGridItem*()> allocation);
    RecyclingGridItem* dequeueReusableCell(std::string identifier);

    // 数据源
    void setDataSource(RecyclingGridDataSource* source);
    RecyclingGridDataSource* getDataSource() const;
    void reloadData();
    void notifyDataChanged();
    void clearData();

    // 焦点
    void setDefaultCellFocus(size_t index);
    size_t getDefaultCellFocus() const;
    void selectRowAt(size_t index, bool animated);
    View* getNextCellFocus(brls::FocusDirection direction, View* currentView);

    // 查询
    RecyclingGridItem* getGridItemByIndex(size_t index);
    std::vector<RecyclingGridItem*>& getGridItems();
    size_t getItemCount();
    size_t getRowCount();
    float getHeightByCellIndex(size_t index, size_t start = 0);

    // 翻页
    void flipScreen(int direction);

    // 分页
    void onNextPage(const std::function<void()>& callback = nullptr);
    void forceRequestNextPage();

    // 焦点变化回调（新增，参考项目没有）
    void setFocusChangeCallback(std::function<void(size_t)> callback);

    // Padding
    void setPadding(float padding) override;
    void setPadding(float top, float right, float bottom, float left) override;
    void setPaddingTop(float top) override;
    void setPaddingRight(float right) override;
    void setPaddingBottom(float bottom) override;
    void setPaddingLeft(float left) override;
    void setPaddingRightPercentage(float right);
    void setPaddingLeftPercentage(float left);
    float getPaddingLeft();
    float getPaddingRight();

    static View* create();

    // 公开配置
    float estimatedRowSpace  = 20;
    float estimatedRowHeight = 240;
    int spanCount            = 4;
    int preFetchLine         = 1;
    bool isFlowMode          = false;

private:
    RecyclingGridDataSource* m_dataSource = nullptr;
    bool m_layouted                       = false;
    float m_oldWidth                      = -1;
    bool m_requestNextPage                = false;

    uint32_t visibleMin = 0, visibleMax = 0;
    size_t m_defaultCellFocus = 0;

    float m_paddingTop    = 0;
    float m_paddingRight  = 0;
    float m_paddingBottom = 0;
    float m_paddingLeft   = 0;
    bool m_paddingPercentage = false;

    std::function<void()> m_nextPageCallback     = nullptr;
    std::function<void(size_t)> m_focusChangeCallback = nullptr;

    RecyclingGridContentBox* m_contentBox = nullptr;
    brls::Rect m_renderedFrame;
    std::vector<float> m_cellHeightCache;
    std::map<std::string, std::vector<RecyclingGridItem*>*> m_queueMap;
    std::map<std::string, std::function<RecyclingGridItem*(void)>> m_allocationMap;

    bool checkWidth();
    void queueReusableCell(RecyclingGridItem* cell);
    void itemsRecyclingLoop();
    void addCellAt(size_t index, bool downSide);
    void removeCell(brls::View* view);
};
