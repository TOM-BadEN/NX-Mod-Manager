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

#include <libretro-common/retro_timers.h>

#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <exception>

#ifdef BOREALIS_USE_STD_THREAD
#include <thread>
#else
#include <pthread.h>
#endif

#ifdef PS4
#include <orbis/libkernel.h>
#endif

namespace brls
{

#ifdef BOREALIS_USE_STD_THREAD
static std::thread *task_loop_thread = nullptr;
#else
static pthread_t task_loop_thread = pthread_t(0);
#endif

Threading::Threading()
{
    start_task_loop();
}

void sync(const std::function<void()>& func)
{
    Threading::sync(func);
}

void async(const std::function<void()>& task)
{
    Threading::async(task);
}

size_t delay(long milliseconds, const std::function<void()>& func)
{
    return Threading::delay(milliseconds, func);
}

void cancelDelay(size_t iter)
{
    Threading::cancelDelay(iter);
}

void Threading::sync(const std::function<void()>& func)
{
    std::lock_guard<std::mutex> guard(m_sync_mutex);
    m_sync_functions.push_back(func);
}

void Threading::async(const std::function<void()>& task)
{
    std::lock_guard<std::mutex> guard(m_async_mutex);
    m_async_tasks.push_back(task);
}

size_t Threading::delay(long milliseconds, const std::function<void()>& func)
{
    std::lock_guard<std::mutex> guard(m_delay_mutex);
    DelayOperation operation;
#ifdef PS4
    operation.startPoint        = sceKernelGetProcessTime();
    operation.delayMilliseconds = milliseconds * 1000;
#else
    operation.startPoint        = std::chrono::high_resolution_clock::now();
    operation.delayMilliseconds = milliseconds;
#endif
    operation.func              = func;
    operation.index             = ++m_delay_index;
    m_delay_tasks.push_back(operation);
    return m_delay_index;
}

void Threading::cancelDelay(size_t iter)
{
    std::lock_guard<std::mutex> guard(m_delay_mutex);
    m_delay_cancel_set.insert(iter);
}

void Threading::performSyncTasks()
{
    m_sync_mutex.lock();
    auto local = m_sync_functions;
    m_sync_functions.clear();
    m_sync_mutex.unlock();

    for (auto& f : local)
    {
        try
        {
            f();
        }
        catch (std::exception& e)
        {
            brls::Logger::error("error: performSyncTasks: {}", e.what());
        }
    }

    m_delay_mutex.lock();
    auto delay_local = m_delay_tasks;
    m_delay_tasks.clear();
    m_delay_mutex.unlock();

    for (auto& d : delay_local)
    {
        // Check cancel
        m_delay_mutex.lock();
        if (m_delay_cancel_set.count(d.index))
        {
            m_delay_cancel_set.erase(d.index);
            m_delay_mutex.unlock();
            continue;
        }
        m_delay_mutex.unlock();

#ifdef PS4
        uint64_t duration = sceKernelGetProcessTime() - d.startPoint;
#else
        auto timeNow  = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(timeNow - d.startPoint).count();
#endif

        if (duration >= d.delayMilliseconds)
        {
            try
            {
                d.func();
            }
            catch (std::exception& e)
            {
                brls::Logger::error("error: performSyncTasks(delay): {}", e.what());
            }

            m_delay_mutex.lock();
            if (m_delay_cancel_set.count(d.index)) m_delay_cancel_set.erase(d.index);
            m_delay_mutex.unlock();

        }
        else
        {
            m_delay_mutex.lock();
            m_delay_tasks.push_back(d);
            m_delay_mutex.unlock();
        }
    }
}

void Threading::start()
{
    task_loop_active = true;
    start_task_loop();
}

void Threading::stop()
{
    task_loop_active = false;

#ifdef BOREALIS_USE_STD_THREAD
    task_loop_thread->join();
    delete task_loop_thread;
    task_loop_thread = nullptr;
#else
    pthread_join(task_loop_thread, NULL);
#endif
}
void Threading::std_task_loop() {
    task_loop(nullptr);
}
void* Threading::task_loop(void* a)
{
    while (task_loop_active)
    {
        std::vector<std::function<void()>> m_tasks_copy;
        {
            std::lock_guard<std::mutex> guard(m_async_mutex);
            m_tasks_copy = m_async_tasks;
            m_async_tasks.clear();
        }

        for (auto task : m_tasks_copy)
        {
            task();
        }

        retro_sleep(500);
    }
    return NULL;
}

void Threading::start_task_loop()
{
#ifdef BOREALIS_USE_STD_THREAD
    task_loop_thread = new std::thread(std_task_loop);
#else
    pthread_create(&task_loop_thread, NULL, task_loop, NULL);
#endif
}

} // namespace brls
