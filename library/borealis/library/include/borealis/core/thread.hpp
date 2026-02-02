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

#include <unistd.h>

#include <chrono>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <set>

namespace brls
{

struct DelayOperation
{
#ifdef PS4
    uint64_t startPoint;
#else
    std::chrono::high_resolution_clock::time_point startPoint;
#endif
    long delayMilliseconds;
    size_t index;
    std::function<void()> func;
};

typedef std::vector<DelayOperation>::iterator DelayOperationIterator;

/**
 * Enqueue a function to be executed before
 * the application is redrawn the next time.
 *
 * Borealis is not thread-safe, and sync() provides a mechanism
 * for queuing up UI-related state changes from other threads.
 *
 * It's a shortcut for brls::Application::sync(&func);
 */
extern void sync(const std::function<void()>& func);

/**
 * Enqueue a function to be executed in
 * parallel with application's main thread.
 *
 * It's a shortcut for brls::Application::async(&func);
 */
extern void async(const std::function<void()>& func);

extern size_t delay(long milliseconds, const std::function<void()>& func);

extern void cancelDelay(size_t iter);

class Threading
{
  public:
    Threading();

    /**
     * Enqueue a function to be executed before
     * the application is redrawn the next time.
     *
     * Borealis is not thread-safe, and sync() provides a mechanism
     * for queuing up UI-related state changes from other threads.
     */
    static void sync(const std::function<void()>& func);

    /**
     * Enqueue a function to be executed in
     * parallel with application's main thread.
     */
    static void async(const std::function<void()>& func);

    static size_t delay(long milliseconds, const std::function<void()>& func);

    static void cancelDelay(size_t iter);

    static void start();

    static void stop();

    static void performSyncTasks();

    static std::vector<std::function<void()>>* getSyncFunctions()
    {
        std::lock_guard<std::mutex> guard(m_sync_mutex);
        return &m_sync_functions;
    }

    static std::vector<std::function<void()>>* getAsyncTasks()
    {
        std::lock_guard<std::mutex> guard(m_async_mutex);
        return &m_async_tasks;
    }

  private:
    inline static std::mutex m_sync_mutex;
    inline static std::vector<std::function<void()>> m_sync_functions;

    inline static std::mutex m_async_mutex;
    inline static std::vector<std::function<void()>> m_async_tasks;

    inline static std::mutex m_delay_mutex;
    inline static std::vector<DelayOperation> m_delay_tasks;
    inline static std::set<size_t> m_delay_cancel_set;
    inline static size_t m_delay_index = 0;

    inline static volatile bool task_loop_active = true;

    static void* task_loop(void* a);
    static void std_task_loop();

    static void start_task_loop();
};

} // namespace brls
