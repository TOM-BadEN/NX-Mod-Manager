/**
 * CapsuleHints - 胶囊样式的全局按键提示栏
 *
 * 复用 Borealis Hints 的 Action 收集、排序和触摸处理，只为其动态创建的
 * 每个 Hint 应用全局外壳胶囊样式。
 */

#pragma once

#include <borealis.hpp>

class CapsuleHints : public brls::Hints {
public:
    CapsuleHints();

    /** @brief 为新建的 Hint 应用胶囊样式后加入提示栏 */
    void addView(brls::View* view) override;

    /** @brief XML View 工厂函数 */
    static brls::View* create();
};
