/**
 * ThreadPool - 通用线程池
 *
 * 固定数量的 worker 线程，接受带 stop_token 的任务。
 * 线程池是纯执行器：取出任务即执行，不负责取消判断。
 * 任务函数须自行检查 stop_token 决定是否提前返回。
 *
 * 两种提交方式：
 *
 * ── submit ──
 *   fire-and-forget，无返回值。
 *   线程池不等待任务结束，可采用以下两种方式保证任务安全：
 *   1. 任务不读取外部对象，将所需数据按值捕获，使任务与提交方生命周期无关。
 *   2. 任务需要读取外部对象，由调用方管理停止和销毁流程，确保对象销毁后，
 *      任务及其回调不再访问该对象；无法保证时应改用 submitWaitable。
 *
 * ── submitWaitable ──
 *   返回 WaitableTask（RAII），允许调用方显式等待任务结束。
 *   WaitableTask 析构时自动 wait，保证任务结束先于句柄销毁。
 *   提交方须在 WaitableTask 析构前 request_stop()。
 */

#pragma once

#include <functional>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <stop_token>
#include <vector>

/** @brief RAII 可等待任务句柄，析构时自动等待任务完成 */
struct WaitableTask {
    /** @brief 创建空任务句柄 */
    WaitableTask() = default;

    /** @brief 等待任务完成后销毁句柄 */
    ~WaitableTask() { wait(); }

    /** @brief 移动构造任务句柄 */
    WaitableTask(WaitableTask&&) = default;

    /**
     * @brief 移动赋值任务句柄
     * @param other 待接管的任务句柄
     * @return 当前任务句柄
     */
    WaitableTask& operator=(WaitableTask&& other) noexcept {
        wait();
        m_future = std::move(other.m_future);
        return *this;
    }

    /** @brief 禁止复制构造 */
    WaitableTask(const WaitableTask&) = delete;

    /** @brief 禁止复制赋值 */
    WaitableTask& operator=(const WaitableTask&) = delete;

    /** @brief 等待任务完成 */
    void wait() { if (m_future.valid()) m_future.wait(); }

private:
    friend class ThreadPool;

    /**
     * @brief 从 future 创建可等待任务句柄
     * @param f 待接管的任务 future
     */
    explicit WaitableTask(std::future<void> f) : m_future(std::move(f)) {}

    std::future<void> m_future; // 任务完成状态
};

/** @brief 固定工作线程数量的通用线程池 */
class ThreadPool {
public:
    /** @brief 获取全局线程池实例 */
    static ThreadPool& instance() {
        static ThreadPool pool(4);
        return pool;
    }

    /**
     * @brief 创建线程池
     * @param workerCount 工作线程数量
     */
    ThreadPool(int workerCount = 3) {
        for (int i = 0; i < workerCount; i++) {
            m_workers.emplace_back([this] { workerLoop(); });
        }
    }

    /** @brief 通知工作线程退出并销毁线程池 */
    ~ThreadPool() {
        {
            std::lock_guard lock(m_mutex);
            m_shutdown = true;
        }
        m_cv.notify_all();
    }

    /** @brief 禁止复制构造 */
    ThreadPool(const ThreadPool&) = delete;

    /** @brief 禁止复制赋值 */
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief 提交无需等待结果的任务，调用方负责被捕获对象的生命周期
     * @param task 任务函数，接受 std::stop_token，须自行检查 token 决定是否提前返回
     * @param token 外部取消令牌，由调用方管理生命周期
     */
    void submit(std::function<void(std::stop_token)> task, std::stop_token token) {
        {
            std::lock_guard lock(m_mutex);
            if (m_shutdown) return;
            m_queue.push({std::move(task), token});
        }
        m_cv.notify_one();
    }

    /**
     * @brief 可等待提交：允许任务函数引用提交方对象，WaitableTask 析构时自动 wait
     * @param task 任务函数，接受 std::stop_token，须自行检查 token 决定是否提前返回
     * @param token 外部取消令牌，由调用方管理生命周期
     * @return WaitableTask RAII 句柄，析构时阻塞等待任务完成
     */
    WaitableTask submitWaitable(std::function<void(std::stop_token)> task, std::stop_token token) {
        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        {
            std::lock_guard lock(m_mutex);
            if (m_shutdown) { promise->set_value(); return WaitableTask(std::move(future)); }
            m_queue.push({[task = std::move(task), promise](std::stop_token tk) {
                task(tk);
                promise->set_value();
            }, token});
        }
        m_cv.notify_one();
        return WaitableTask(std::move(future));
    }

private:
    /** @brief 待执行的线程池任务 */
    struct Job {
        std::function<void(std::stop_token)> task; // 待执行的任务函数
        std::stop_token token;                     // 调用方传入的取消令牌
    };

    /** @brief worker 线程主循环：等待任务 → 取出 → 执行，直到池关闭 */
    void workerLoop() {
        while (true) {
            Job job;
            {
                std::unique_lock lock(m_mutex);
                // 无任务时挂起，等待 submit 或 shutdown 唤醒
                m_cv.wait(lock, [this] { return m_shutdown || !m_queue.empty(); });
                if (m_shutdown && m_queue.empty()) return;
                job = std::move(m_queue.front());
                m_queue.pop();
            }
            job.task(job.token);
        }
    }

    std::vector<std::jthread> m_workers; // 固定数量的 worker 线程（析构时自动 join）
    std::queue<Job> m_queue;             // 任务队列（先进先出）
    std::mutex m_mutex;                  // 保护 m_queue 和 m_shutdown 的互斥锁
    std::condition_variable m_cv;        // 用于唤醒空闲 worker
    bool m_shutdown = false;             // 为 true 时 worker 不再取新任务，准备退出
};
