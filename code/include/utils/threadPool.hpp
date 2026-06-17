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
 *   任务函数不得持有对提交方对象的引用，所有数据须以值捕获。
 *   提交方析构时仅需 request_stop()，无需等待任务完成。
 *
 * ── submitWaitable ──
 *   返回 WaitableTask（RAII），允许任务函数引用提交方对象。
 *   WaitableTask 析构时自动 wait，保证任务结束先于对象销毁。
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
    WaitableTask() = default;
    ~WaitableTask() { wait(); }

    WaitableTask(WaitableTask&&) = default;
    WaitableTask& operator=(WaitableTask&& other) noexcept {
        wait();
        m_future = std::move(other.m_future);
        return *this;
    }

    WaitableTask(const WaitableTask&) = delete;
    WaitableTask& operator=(const WaitableTask&) = delete;

    void wait() { if (m_future.valid()) m_future.wait(); }

private:
    friend class ThreadPool;
    explicit WaitableTask(std::future<void> f) : m_future(std::move(f)) {}
    std::future<void> m_future;
};

class ThreadPool {
public:
    static ThreadPool& instance() {
        static ThreadPool pool(4);
        return pool;
    }

    ThreadPool(int workerCount = 3) {
        for (int i = 0; i < workerCount; i++) {
            m_workers.emplace_back([this] { workerLoop(); });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(m_mutex);
            m_shutdown = true;
        }
        m_cv.notify_all();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief fire-and-forget 提交：任务函数不得持有对提交方对象的引用
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
    struct Job {
        std::function<void(std::stop_token)> task;  // 待执行的任务函数
        std::stop_token token;                      // 调用方传入的取消令牌
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

    std::vector<std::jthread> m_workers;  // 固定数量的 worker 线程（析构时自动 join）
    std::queue<Job> m_queue;               // 任务队列（先进先出）
    std::mutex m_mutex;                    // 保护 m_queue 和 m_shutdown 的互斥锁
    std::condition_variable m_cv;          // 用于唤醒空闲 worker
    bool m_shutdown = false;               // 为 true 时 worker 不再取新任务，准备退出
};

