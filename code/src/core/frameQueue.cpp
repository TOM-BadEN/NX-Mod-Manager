/**
 * FrameQueue - 全局逐帧任务队列实现
 */

#include "core/frameQueue.hpp"
#include <utility>

std::mutex FrameQueue::m_mutex;
std::deque<FrameQueue::Task> FrameQueue::m_tasks;
std::size_t FrameQueue::m_tasksPerFrame = FrameQueue::MAX_TASKS_PER_FRAME;
brls::Time FrameQueue::m_lastFrameTime = 0;
std::size_t FrameQueue::m_stableFrameCount = 0;
bool FrameQueue::m_acceptingTasks = false;
bool FrameQueue::m_initialized = false;
brls::VoidEvent::Subscription FrameQueue::m_subscription;

void FrameQueue::initialize() {
    if (m_initialized) return;

    m_subscription = brls::Application::getRunLoopEvent()->subscribe([] { processFrame(); });
    {
        std::lock_guard lock(m_mutex);
        m_acceptingTasks = true;
    }
    m_tasksPerFrame = MAX_TASKS_PER_FRAME;
    m_lastFrameTime = 0;
    m_stableFrameCount = 0;
    m_initialized = true;
}

void FrameQueue::shutdown() {
    if (!m_initialized) return;

    {
        std::lock_guard lock(m_mutex);
        m_acceptingTasks = false;
        m_tasks.clear();
    }
    brls::Application::getRunLoopEvent()->unsubscribe(m_subscription);
    m_tasksPerFrame = MAX_TASKS_PER_FRAME;
    m_lastFrameTime = 0;
    m_stableFrameCount = 0;
    m_initialized = false;
}

bool FrameQueue::enqueue(std::stop_token token, Callback callback) {
    if (!callback || token.stop_requested()) return false;

    std::lock_guard lock(m_mutex);
    if (!m_acceptingTasks) return false;

    m_tasks.push_back({token, std::move(callback)});
    return true;
}

void FrameQueue::processFrame() {
    brls::Time currentTime = brls::getCPUTimeUsec();
    if (m_lastFrameTime != 0) {
        brls::Time frameTime = currentTime - m_lastFrameTime;
        if (frameTime > SLOW_FRAME_TIME_US) {
            m_tasksPerFrame = MIN_TASKS_PER_FRAME;
            m_stableFrameCount = 0;
        } else if (frameTime <= RECOVER_FRAME_TIME_US && m_tasksPerFrame == MIN_TASKS_PER_FRAME) {
            m_stableFrameCount++;
            if (m_stableFrameCount >= RECOVER_FRAME_COUNT) {
                m_tasksPerFrame = MAX_TASKS_PER_FRAME;
                m_stableFrameCount = 0;
            }
        } else {
            m_stableFrameCount = 0;
        }
    }
    m_lastFrameTime = currentTime;

    std::size_t processedCount = 0;
    while (processedCount < m_tasksPerFrame) {
        Task task;
        {
            std::lock_guard lock(m_mutex);
            if (m_tasks.empty()) return;
            task = std::move(m_tasks.front());
            m_tasks.pop_front();
        }

        if (task.token.stop_requested()) continue;

        task.callback();
        processedCount++;
    }
}
