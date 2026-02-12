/**
 * IndexUpdate - 索引更新工具
 * 管理选中索引 + 总数 + 回调通知，各页面复用
 */

#pragma once

#include <functional>

class IndexUpdate {
public:
    void setCallback(std::function<void(int, int)> cb) { m_callback = cb; }
    
    // 更新索引和总数，自动通知（内部 0-based，回调传出 1-based）
    void update(int index, int total) {
        m_index = index;
        m_total = total;
        notify();
    }
    
    void notify() {
        if (m_callback) m_callback(m_index + 1, m_total);
    }
    
    int getIndex() const { return m_index + 1; }
    int getTotal() const { return m_total; }

private:
    int m_index = 0;
    int m_total = 0;
    std::function<void(int, int)> m_callback;
};
