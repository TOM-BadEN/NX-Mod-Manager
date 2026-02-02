/*
    Copyright 2021 XITRIX

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#pragma once

#include <borealis/core/application.hpp>
#include <borealis/core/bind.hpp>
#include <borealis/views/header.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/rectangle.hpp>
#include <borealis/views/scrolling_frame.hpp>
#include <functional>
#include <map>
#include <vector>

namespace brls
{

struct IndexPath
{
    size_t section;
    int row;
    size_t item;

    IndexPath()
        : IndexPath(0, 0, 0)
    {
    }

    IndexPath(size_t section, size_t row, size_t item)
    {
        this->section = section;
        this->row     = row;
        this->item    = item;
    }

    IndexPath(size_t section, size_t row)
        : IndexPath(section, row, row)
    {
    }

    bool operator==(const IndexPath& other)
    {
        return section == other.section && row == other.row && item == other.item;
    }
};

class RecyclerCell : public Box
{
  public:
    RecyclerCell();
    ~RecyclerCell();

    /*
     * Cell's position inside recycler frame
     */
    IndexPath getIndexPath() const { return indexPath; }

    /*
     * DO NOT USE! FOR INTERNAL USAGE ONLY!
     */
    void setIndexPath(IndexPath value);

    /*
     * A string used to identify a cell that is reusable.
     */
    std::string reuseIdentifier;

    /*
     * Prepares a reusable cell for reuse by the recycler frame's data source.
     */
    virtual void prepareForReuse() { }

    static RecyclerCell* create();

    void onFocusGained() override;
    void onFocusLost() override;

  private:
    IndexPath indexPath;
    Event<InputType>::Subscription subscription;
};

class RecyclerHeader
    : public RecyclerCell
{
  public:
    RecyclerHeader();

    void setTitle(std::string text);
    void setSubtitle(std::string text);

    static RecyclerHeader* create();

  private:
    Header* header;
};

class RecyclerFrame;
class RecyclerDataSource
{
  public:
    /*
     * Asks the data source to return the number of sections in the recycler frame.
     */
    virtual int numberOfSections(RecyclerFrame* recycler) { return 1; }

    /*
     * Tells the data source to return the number of rows in a recycler frame.
     */
    virtual int numberOfRows(RecyclerFrame* recycler, int section) { return 0; }

    /*
     * Asks the data source for a cell to insert in a particular location of the recycler frame.
     */
    virtual RecyclerCell* cellForRow(RecyclerFrame* recycler, IndexPath index) { return nullptr; }

    /*
     * Asks the data source for a cell to display in the header of the specified section of the recycler frame.
     */
    virtual RecyclerCell* cellForHeader(RecyclerFrame* recycler, int section);

    /*
     * Asks the data source for the height to use for a row in a specified location.
     * Return -1 to use autoscaling.
     */
    virtual float heightForRow(RecyclerFrame* recycler, IndexPath index) { return -1; }

    /*
     * Asks the data source for the height to use for the header of a particular section.
     * Return -1 to use autoscaling.
     */
    virtual float heightForHeader(RecyclerFrame* recycler, int section);

    /*
     * Asks the data source for the title of the header of the specified section of the recycler frame.
     */
    virtual std::string titleForHeader(RecyclerFrame* recycler, int section) { return ""; }

    /*
     * Tells the data source a row is selected.
     */
    virtual void didSelectRowAt(RecyclerFrame* recycler, IndexPath index) { }

    virtual ~RecyclerDataSource() = default;
};

class RecyclerContentBox : public Box
{
  public:
    RecyclerContentBox(RecyclerFrame* recycler);
    View* getNextFocus(FocusDirection direction, View* currentView) override;

  private:
    RecyclerFrame* recycler;
};

// Custom Box for propper recycling navigation
class RecyclerFrame : public ScrollingFrame
{
  public:
    RecyclerFrame();
    ~RecyclerFrame();

    View* getNextCellFocus(FocusDirection direction, View* currentView);
    void draw(NVGcontext* vg, float x, float y, float width, float height, Style style, FrameContext* ctx) override;
    void onLayout() override;
    void setPadding(float padding) override;
    void setPadding(float top, float right, float bottom, float left) override;
    void setPaddingTop(float top) override;
    void setPaddingRight(float right) override;
    void setPaddingBottom(float bottom) override;
    void setPaddingLeft(float left) override;

    /*
     * Set an object that acts as the data source of the recycler frame.
     *
     * deleteDataSource: set to false if source object deletion should NOT be
     * handled by the RecyclerFrame.
     */
    void setDataSource(RecyclerDataSource* source, bool deleteDataSource = true);

    /*
     * Get an object that acts as the data source of the recycler frame.
     */
    RecyclerDataSource* getDataSource() const;

    /*
     * Reloads the rows of the recycler frame.
     */
    void reloadData();

    /*
     * Registers a class for use in creating new recycler cells.
     */
    void registerCell(std::string identifier, std::function<RecyclerCell*(void)> allocation);

    /*
     * Returns a reusable recycler-frame cell object for the specified reuse identifier
     */
    RecyclerCell* dequeueReusableCell(std::string identifier);

    /*
     * Selects a row in the recycler frame identified by index path.
     */
    void selectRowAt(IndexPath indexPath, bool animated);

    /*
     * Used for initial recycler's frame calculation if rows autoscaling selected.
     * To provide more accurate height implement DataSource->cellHeightForRow().
     */
    float estimatedRowHeight = 44;

    IndexPath getDefaultCellFocus()
    {
        return this->defaultCellFocus;
    }

    void setDefaultCellFocus(IndexPath indexPath)
    {
        this->defaultCellFocus = indexPath;
    }

    static View* create();

  private:
    RecyclerDataSource* dataSource = nullptr;
    bool deleteDataSource          = false;
    bool layouted                  = false;

    uint32_t visibleMin, visibleMax;

    IndexPath defaultCellFocus;

    float paddingTop    = 0;
    float paddingRight  = 0;
    float paddingBottom = 0;
    float paddingLeft   = 0;

    Box* contentBox;
    Rect renderedFrame;
    std::vector<Size> cacheFramesData;
    std::vector<IndexPath> cacheIndexPathData;
    std::map<std::string, std::vector<RecyclerCell*>*> queueMap;
    std::map<std::string, std::function<RecyclerCell*(void)>> allocationMap;

    bool checkWidth();

    void cacheCellFrames();
    void cellsRecyclingLoop();
    void queueReusableCell(RecyclerCell* cell);

    void addCellAt(size_t index, size_t downSide);
    void removeCell(View* view);
};

} // namespace brls
