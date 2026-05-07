/**
 * RecyclingGrid - 通用回收网格组件
 * 继承 ScrollingFrame，支持多列网格布局 + Cell 回收复用 + 分页加载
 * 移植自 wiliwili 项目，移除了 ButtonRefresh / SkeletonCell 等特有依赖，添加了内置翻页功能，
 * 修改了部分代码逻辑
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

    void onFocusGained() override;
    void onFocusLost() override;

    /** @brief 获取索引 */
    size_t getIndex() const;

    /**
     * @brief 设置索引
     * @param value 索引值
     */
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

    /**
     * @brief 注册 Cell 类型
     * @param identifier 复用标识
     * @param allocation 工厂函数
     */
    void registerCell(std::string identifier, std::function<RecyclingGridItem*()> allocation);

    /**
     * @brief 获取可复用的 Cell
     * @param identifier 复用标识
     */
    RecyclingGridItem* dequeueReusableCell(std::string identifier);

    /**
     * @brief 设置数据源
     * @param source 数据源
     */
    void setDataSource(RecyclingGridDataSource* source);

    /** @brief 获取数据源 */
    RecyclingGridDataSource* getDataSource() const;

    /** @brief 重新加载数据 */
    void reloadData();

    /**
     * @brief 延迟重新加载
     * @param focusIndex 焦点索引
     */
    void deferReload(int focusIndex);

    /** @brief 通知数据已变更 */
    void notifyDataChanged();

    /** @brief 清空数据 */
    void clearData();

    /**
     * @brief 设置默认焦点 Cell
     * @param index 索引
     */
    void setDefaultCellFocus(size_t index);

    /** @brief 获取默认焦点索引 */
    size_t getDefaultCellFocus() const;

    /**
     * @brief 选中指定行
     * @param index 索引
     * @param animated 是否动画
     */
    void selectRowAt(size_t index, bool animated);

    /**
     * @brief 立即聚焦到指定索引
     * @param index 索引
     */
    void instantFocus(size_t index);

    /**
     * @brief 获取下一个 Cell 焦点
     * @param direction 方向
     * @param currentView 当前视图
     */
    View* getNextCellFocus(brls::FocusDirection direction, View* currentView);

    /**
     * @brief 按索引获取 Cell
     * @param index 索引
     */
    RecyclingGridItem* getGridItemByIndex(size_t index);

    /** @brief 获取所有可见 Cell */
    std::vector<RecyclingGridItem*>& getGridItems();

    /** @brief 获取条目总数 */
    size_t getItemCount();

    /** @brief 获取行数 */
    size_t getRowCount();

    /**
     * @brief 获取指定 Cell 的累计高度
     * @param index Cell 索引
     * @param start 起始索引
     */
    float getHeightByCellIndex(size_t index, size_t start = 0);

    /**
     * @brief 翻页
     * @param direction 方向（1=下一页，-1=上一页）
     */
    void flipScreen(int direction);

    /**
     * @brief 设置下一页加载回调
     * @param callback 回调函数
     */
    void onNextPage(const std::function<void()>& callback = nullptr);

    /** @brief 强制请求下一页 */
    void forceRequestNextPage();

    /**
     * @brief 设置焦点变化回调
     * @param callback 回调函数
     */
    void setFocusChangeCallback(std::function<void(size_t)> callback);

    /** @brief 设置四周等距 Padding */
    void setPadding(float padding) override;

    /**
     * @brief 设置四侧 Padding
     * @param top 上
     * @param right 右
     * @param bottom 下
     * @param left 左
     */
    void setPadding(float top, float right, float bottom, float left) override;

    void setPaddingTop(float top) override;
    void setPaddingRight(float right) override;
    void setPaddingBottom(float bottom) override;
    void setPaddingLeft(float left) override;

    /**
     * @brief 设置右侧百分比 Padding
     * @param right 百分比
     */
    void setPaddingRightPercentage(float right);

    /**
     * @brief 设置左侧百分比 Padding
     * @param left 百分比
     */
    void setPaddingLeftPercentage(float left);

    /** @brief 获取左侧 Padding */
    float getPaddingLeft();

    /** @brief 获取右侧 Padding */
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
    int m_deferredFocusIndex  = -1;

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

    /** @brief 检查宽度是否变化 */
    bool checkWidth();

    /**
     * @brief 回收 Cell 到复用队列
     * @param cell 待回收 Cell
     */
    void queueReusableCell(RecyclingGridItem* cell);

    /** @brief Cell 回收循环 */
    void itemsRecyclingLoop();

    /**
     * @brief 在指定位置添加 Cell
     * @param index 索引
     * @param downSide 是否添加到下方
     */
    void addCellAt(size_t index, bool downSide);

    /**
     * @brief 移除 Cell
     * @param view 待移除视图
     */
    void removeCell(brls::View* view);
};
