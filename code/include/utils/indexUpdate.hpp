/**
 * IndexUpdate - 索引更新工具
 * 管理选中索引 + 总数 + 回调通知，各页面复用
 */

#pragma once

#include <functional>

class IndexUpdate {
public:
    /**
     * @brief 设置更新回调
     * @param cb 回调函数（当前索引, 总数）
     */
    void setCallback(std::function<void(int, int)> cb) {
        m_callback = cb;
    }
    
    /**
     * @brief 更新索引和总数，自动通知（内部 0-based，回调传出 1-based）
     * @param index 当前索引
     * @param total 总数
     */
    void update(int index, int total) {
        m_index = index;
        m_total = total;
        notify();
    }
    
    /** @brief 手动触发通知 */
    void notify() {
        if (m_callback) m_callback(m_index + 1, m_total);
    }
    
    /** @brief 获取当前索引（1-based） */
    int getIndex() const {
        return m_index + 1;
    }
    
    /** @brief 获取总数 */
    int getTotal() const {
        return m_total;
    }

private:
    int m_index = 0;
    int m_total = 0;
    std::function<void(int, int)> m_callback;
};
