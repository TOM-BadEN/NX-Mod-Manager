/**
 * FrameQueue - 全局逐帧任务队列
 *
 * 接收任意主线程回调，并根据当前帧率限制每帧执行的任务数量。
 * 队列只负责任务调度，不负责后台工作、业务处理和资源所有权。
 */

#pragma once

#include <borealis.hpp>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <stop_token>

class FrameQueue {
public:
    using Callback = std::function<void()>;

    /** @brief 在主线程初始化逐帧处理 */
    static void initialize();

    /** @brief 停止接收任务并清空尚未执行的回调 */
    static void shutdown();

    /**
     * @brief 提交一项主线程任务
     * @param token 页面管理的取消令牌
     * @param callback 需要在主线程执行的回调
     * @return 队列已初始化且任务成功加入时返回 true
     */
    static bool enqueue(std::stop_token token, Callback callback);

private:
    /** @brief 单帧允许执行的任务数量范围 */
    static constexpr std::size_t MIN_TASKS_PER_FRAME = 1;
    static constexpr std::size_t MAX_TASKS_PER_FRAME = 2;

    /** @brief 动态调整单帧任务数量的帧耗时阈值 */
    static constexpr brls::Time SLOW_FRAME_TIME_US    = 17500;
    static constexpr brls::Time RECOVER_FRAME_TIME_US = 17000;
    static constexpr std::size_t RECOVER_FRAME_COUNT  = 3;

    struct Task {
        std::stop_token token;
        Callback callback;
    };

    /** @brief 在当前帧执行限定数量的任务 */
    static void processFrame();

    static std::mutex m_mutex;
    static std::deque<Task> m_tasks;
    static std::size_t m_tasksPerFrame;
    static brls::Time m_lastFrameTime;
    static std::size_t m_stableFrameCount;
    static bool m_acceptingTasks;
    static bool m_initialized;
    static brls::VoidEvent::Subscription m_subscription;
};
