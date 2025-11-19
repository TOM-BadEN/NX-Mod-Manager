// 应用程序主头文件 (Main application header file)
#include "app.hpp"
// NanoVG工具函数 (NanoVG utility functions)
#include "nvg_util.hpp"
// NanoVG Deko3D渲染器 (NanoVG Deko3D renderer)
#include "nanovg/deko3d/nanovg_dk.h"
// STB图像库 (STB image library)
#include "nanovg/stb_image.h"
// 语言管理器 (Language manager)
#include "lang_manager.hpp"
// JSON管理器 (JSON manager)
#include "json_manager.hpp"
// 虚拟键盘辅助工具 (Virtual keyboard helper)
#include "keyboard_helper.hpp"
// 拼音库 (Pinyin library)
#include "Pinyin-onefile.cpp"
// 时间计算 (Time calculation)
#include <chrono>

// 原子操作 (Atomic operations)
#include <atomic>
// 智能指针 (Smart pointers)
#include <memory>
// C字符串操作 (C string operations)
#include <cstring>
// Nintendo Switch应用缓存库 (Nintendo Switch application cache library)
#include <nxtc_version.h>

// 算法库 (Algorithm library)
#include <algorithm>
// 范围库 (Ranges library)
#include <ranges>
// 迭代器库 (Iterator library)
#include <iterator>
// 目录操作 (Directory operations)
#include <dirent.h>
// 系统调用 (System calls)
#include <cstdlib>
// Switch文件系统API (Switch filesystem API)
#include <switch.h>
// 文件状态 (File status)
#include <sys/stat.h>
// 错误码定义 (Error code definitions)
#include <errno.h>
// 字符串流 (String stream)
#include <sstream>



// 调试模式日志宏定义 (Debug mode log macro definition)
#ifndef NDEBUG
    #include <cstdio>
    #define LOG(...) std::printf(__VA_ARGS__)
#else // NDEBUG
    #define LOG(...)
#endif // NDEBUG

namespace tj {

// App类的额外成员变量 (Additional member variables for App class)
// 标记首批应用是否已加载完成 (Flag indicating if initial batch of apps is loaded)
std::atomic<bool> App::initial_batch_loaded{false};
// 已扫描的应用数量 (Number of scanned applications)
std::atomic<size_t> App::scanned_count{0};
// 应用总数量 (Total number of applications)
std::atomic<size_t> App::total_count{0};
// 标记扫描是否正在运行 (Flag indicating if scanning is running)
std::atomic<bool> App::is_scan_running{true};

// ADDGAMELIST界面专用的扫描状态变量 (Dedicated scanning state variables for ADDGAMELIST interface)
std::atomic<size_t> App::addgame_scanned_count{0};
std::atomic<size_t> App::addgame_total_count{0};
std::atomic<bool> App::addgame_scan_running{false};

// 保护应用条目列表的互斥锁 (Mutex to protect application entries list)
std::mutex App::entries_mutex;
std::mutex App::entries_AddGame_mutex;

namespace {





// 感谢Shchmue的贡献 (Thank you Shchmue ^^)
// 应用程序占用空间条目结构体 (Application occupied size entry structure)
struct ApplicationOccupiedSizeEntry {
    std::uint8_t storageId;        // 存储设备ID (Storage device ID)
    std::uint64_t sizeApplication; // 应用程序本体大小 (Application base size)
    std::uint64_t sizePatch;       // 补丁大小 (Patch size)
    std::uint64_t sizeAddOnContent; // 追加内容大小 (Add-on content size)
};

// 应用程序占用空间结构体 (Application occupied size structure)
struct ApplicationOccupiedSize {
    ApplicationOccupiedSizeEntry entry[4]; // 最多4个存储设备的条目 (Up to 4 storage device entries)
};

// 屏幕尺寸常量 (Screen dimension constants)
constexpr float SCREEN_WIDTH = 1280.f;  // 屏幕宽度 (Screen width)
constexpr float SCREEN_HEIGHT = 720.f;  // 屏幕高度 (Screen height)
constexpr int BATCH_SIZE = 4; // 首批加载的应用数量 (Initial batch size for loading apps)

// 验证JPEG数据完整性的辅助函数
// Helper function to validate JPEG data integrity
bool IsValidJpegData(const std::vector<unsigned char>& data) {
    if (data.size() < 4) return false;
    
    // 检查JPEG文件头和文件尾
    // Check JPEG file header and trailer
    bool has_jpeg_header = (data[0] == 0xFF && data[1] == 0xD8);
    bool has_jpeg_trailer = (data[data.size()-2] == 0xFF && data[data.size()-1] == 0xD9);
    return has_jpeg_header && has_jpeg_trailer;
}


// 异步删除应用程序函数 (Asynchronous application deletion function)
// 来自游戏卡安装器的脉冲颜色结构体 (Pulse color structure from gamecard installer)
struct PulseColour {
    NVGcolor col{0, 255, 187, 255}; // 颜色值 (Color value)
    u8 delay;                       // 延迟时间 (Delay time)
    bool increase_blue;             // 是否增加蓝色分量 (Whether to increase blue component)
};

PulseColour pulse; //脉冲颜色对象(Pulse color object)

void update_pulse_colour() { //更新脉冲颜色函数(Update pulse color function)
    if (pulse.col.g == 255) { //当绿色分量达到最大值时(When green component reaches maximum)
        pulse.increase_blue = true; //开始增加蓝色分量(Start increasing blue component)
    } else if (pulse.col.b == 255 && pulse.delay == 10) { //当蓝色分量达到最大值且延迟达到10时(When blue component reaches maximum and delay reaches 10)
        pulse.increase_blue = false; //停止增加蓝色分量(Stop increasing blue component)
        pulse.delay = 0; //重置延迟计数器(Reset delay counter)
    }

    
    
    if (pulse.col.b == 255 && pulse.increase_blue == true) { //当蓝色分量为最大值且正在增加蓝色时(When blue component is at maximum and increasing blue)
        pulse.delay++; //增加延迟计数器(Increment delay counter)
    } else {
        pulse.col.b = pulse.increase_blue ? pulse.col.b + 2 : pulse.col.b - 2; //根据方向调整蓝色分量(Adjust blue component based on direction)
        pulse.col.g = pulse.increase_blue ? pulse.col.g - 2 : pulse.col.g + 2; //根据方向调整绿色分量(Adjust green component based on direction)
    }
}

} // namespace

// 字符串格式化辅助函数实现 (String formatting helper function implementations)
std::string App::GetSnprintf(const std::string& format_str, const std::string& value) {
    // 计算所需缓冲区大小 (Calculate required buffer size)
    int size = std::snprintf(nullptr, 0, format_str.c_str(), value.c_str());
    if (size <= 0) return format_str; // 格式化失败，返回原字符串 (Format failed, return original string)
    
    // 分配缓冲区并格式化 (Allocate buffer and format)
    std::vector<char> buffer(size + 1);
    std::snprintf(buffer.data(), buffer.size(), format_str.c_str(), value.c_str());
    return std::string(buffer.data());
}

std::string App::GetSnprintf(const std::string& format_str, const std::string& value1, const std::string& value2) {
    // 计算所需缓冲区大小 (Calculate required buffer size)
    int size = std::snprintf(nullptr, 0, format_str.c_str(), value1.c_str(), value2.c_str());
    if (size <= 0) return format_str; // 格式化失败，返回原字符串 (Format failed, return original string)
    
    // 分配缓冲区并格式化 (Allocate buffer and format)
    std::vector<char> buffer(size + 1);
    std::snprintf(buffer.data(), buffer.size(), format_str.c_str(), value1.c_str(), value2.c_str());
    return std::string(buffer.data());
}

std::string App::GetSnprintf(const std::string& format_str, const std::string& value1, const std::string& value2, const std::string& value3) {
    // 计算所需缓冲区大小 (Calculate required buffer size)
    int size = std::snprintf(nullptr, 0, format_str.c_str(), value1.c_str(), value2.c_str(), value3.c_str());
    if (size <= 0) return format_str; // 格式化失败，返回原字符串 (Format failed, return original string)
    
    // 分配缓冲区并格式化 (Allocate buffer and format)
    std::vector<char> buffer(size + 1);
    std::snprintf(buffer.data(), buffer.size(), format_str.c_str(), value1.c_str(), value2.c_str(), value3.c_str());
    return std::string(buffer.data());
}

// 格式化时间显示函数 (Format duration display function)
std::string App::FormatDuration(double seconds) {
    // 如果小于1秒，
    if (seconds < 1.0) {
        return "1s";
    }
    
    int total_seconds = static_cast<int>(seconds);
    int hours = total_seconds / 3600;           // 计算小时数 (Calculate hours)
    int minutes = (total_seconds % 3600) / 60;  // 计算分钟数 (Calculate minutes)
    int secs = total_seconds % 60;              // 计算秒数 (Calculate seconds)
    
    if (hours > 0) {
        // xx时xx分xx秒 (xx hours xx minutes xx seconds)
        return std::to_string(hours) + "h" + std::to_string(minutes) + "m" + std::to_string(secs) + "s";
    } else if (minutes > 0) {
        // xx分xx秒 (xx minutes xx seconds)
        return std::to_string(minutes) + "m" + std::to_string(secs) + "s";
    } else {
        // xx秒 (xx seconds)
        return std::to_string(secs) + "s";
    }
}


// ResourceLoadManager 实现 (ResourceLoadManager implementation)
// 资源加载管理器的具体实现 (Concrete implementation of resource load manager)
void ResourceLoadManager::submitLoadTask(const ResourceLoadTask& task) { // 提交加载任务 (Submit loading task)
    std::scoped_lock lock{task_mutex}; // 获取互斥锁保护任务队列 (Acquire mutex lock to protect task queue)
    pending_tasks.push(task); // 将任务添加到待处理队列 (Add task to pending queue)
}

void ResourceLoadManager::processFrameLoads() { // 处理每帧的加载任务 (Process loading tasks per frame)
    int icon_loads_this_frame = 0; // 当前帧已加载的图标数量 (Number of icons loaded in current frame)
    
    std::scoped_lock lock{task_mutex}; // 获取互斥锁保护任务队列 (Acquire mutex lock to protect task queue)
    
    // 处理任务，只对图标任务限制每帧最多2个 (Process tasks, only limit icon tasks to max 2 per frame)
    // 这样可以避免图标加载造成的帧率下降 (This prevents frame rate drops caused by icon loading)
    while (!pending_tasks.empty()) { // 当队列不为空时继续处理 (Continue processing while queue is not empty)
        auto task = pending_tasks.top(); // 获取优先级最高的任务 (Get highest priority task)
        
        // 如果是图标任务且已达到每帧图标加载限制，则停止处理图标任务 (If it's an icon task and we've reached the per-frame icon loading limit, stop processing icon tasks)
        // 但仍然尝试处理其他类型的任务以保持响应性 (But still try to process other types of tasks to maintain responsiveness)
        if (task.task_type == ResourceTaskType::ICON && icon_loads_this_frame >= MAX_ICON_LOADS_PER_FRAME) {
            // 检查是否还有非图标任务可以处理 (Check if there are non-icon tasks that can be processed)
            // 这确保了非图标任务不会被图标任务阻塞 (This ensures non-icon tasks are not blocked by icon tasks)
            std::vector<ResourceLoadTask> temp_tasks; // 临时存储任务的容器 (Temporary container for storing tasks)
            bool found_non_icon = false; // 是否找到非图标任务的标志 (Flag indicating if non-icon task is found)
            
            // 临时移除任务来查找非图标任务 (Temporarily remove tasks to find non-icon tasks)
            // 这是一个线性搜索过程，但任务队列通常不会很大 (This is a linear search process, but task queue is usually not large)
            while (!pending_tasks.empty()) {
                auto temp_task = pending_tasks.top(); // 获取队列顶部任务 (Get top task from queue)
                pending_tasks.pop(); // 从队列中移除任务 (Remove task from queue)
                temp_tasks.push_back(temp_task); // 添加到临时容器 (Add to temporary container)
                
                if (temp_task.task_type != ResourceTaskType::ICON) { // 如果不是图标任务 (If it's not an icon task)
                    found_non_icon = true; // 标记找到非图标任务 (Mark that non-icon task is found)
                    task = temp_task; // 使用找到的非图标任务 (Use the found non-icon task)
                    temp_tasks.pop_back(); // 移除找到的非图标任务 (Remove the found non-icon task)
                    break; // 跳出搜索循环 (Break out of search loop)
                }
            }
            
            // 将临时移除的任务放回队列 (Put temporarily removed tasks back to queue)
            // 保持任务队列的完整性 (Maintain integrity of task queue)
            for (const auto& temp_task : temp_tasks) {
                pending_tasks.push(temp_task); // 重新添加到队列 (Re-add to queue)
            }
            
            if (!found_non_icon) { // 如果没有找到非图标任务 (If no non-icon task is found)
                break; // 没有可处理的任务，退出循环 (No processable tasks, exit loop)
            }
        } else {
            pending_tasks.pop(); // 从队列中移除当前任务 (Remove current task from queue)
        }
        
        // 执行加载任务 (Execute loading task)
        // 调用任务的回调函数来执行实际的加载操作 (Call task's callback function to perform actual loading operation)
        if (task.load_callback) { // 如果回调函数存在 (If callback function exists)
            task.load_callback(); // 执行回调函数 (Execute callback function)
        }
        
        // 只对图标任务计数 (Only count icon tasks)
        // 这样可以准确控制每帧的图标加载数量 (This allows accurate control of icon loading count per frame)
        if (task.task_type == ResourceTaskType::ICON) {
            icon_loads_this_frame++; // 增加图标加载计数 (Increment icon loading count)
        }
    }
    
    
}

bool ResourceLoadManager::hasPendingTasks() const { // 检查是否有待处理任务 (Check if there are pending tasks)
    std::scoped_lock lock{task_mutex}; // 获取互斥锁保护任务队列 (Acquire mutex lock to protect task queue)
    return !pending_tasks.empty(); // 返回队列是否非空 (Return whether queue is not empty)
}

size_t ResourceLoadManager::getPendingTaskCount() const { // 获取待处理任务数量 (Get pending task count)
    std::scoped_lock lock{task_mutex}; // 获取互斥锁保护任务队列 (Acquire mutex lock to protect task queue)
    return pending_tasks.size(); // 返回队列大小 (Return queue size)
}

void App::Loop() { // 应用程序主循环方法 (Application main loop method)
    // 60FPS限制：每帧16.666667毫秒 (Frame rate limit: 16.666667ms per frame for 60FPS)
    // 这确保了游戏在Switch上的流畅运行 (This ensures smooth gameplay on Switch)
    constexpr auto target_frame_time = std::chrono::microseconds(16667); // 1/60秒 (1/60 second)
    auto last_frame_time = std::chrono::steady_clock::now(); // 记录上一帧的时间戳 (Record timestamp of last frame)
    
    // 主循环：持续运行直到退出或applet停止 (Main loop: continue running until quit or applet stops)
    // appletMainLoop()检查Switch系统是否允许应用继续运行 (appletMainLoop() checks if Switch system allows app to continue)
    while (!this->quit && appletMainLoop()) {
        auto frame_start = std::chrono::steady_clock::now(); // 记录当前帧开始时间 (Record current frame start time)
        
        // 执行每帧的核心操作 (Execute core operations per frame)
        this->Poll(); // 处理输入事件 (Process input events)
        this->Update(); // 更新游戏逻辑 (Update game logic)
        this->Draw(); // 渲染画面 (Render graphics)
        
        // 计算帧时间并限制到60FPS (Calculate frame time and limit to 60FPS)
        // 这是帧率控制的关键部分 (This is the key part of frame rate control)
        auto frame_end = std::chrono::steady_clock::now(); // 记录当前帧结束时间 (Record current frame end time)
        auto frame_duration = frame_end - frame_start; // 计算帧处理耗时 (Calculate frame processing time)
        
        // 如果帧处理时间少于目标时间，则休眠剩余时间 (If frame processing time is less than target, sleep for remaining time)
        // 这确保了稳定的60FPS帧率 (This ensures stable 60FPS frame rate)
        if (frame_duration < target_frame_time) {
            auto sleep_time = target_frame_time - frame_duration; // 计算需要休眠的时间 (Calculate sleep time needed)
            auto sleep_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_time).count(); // 转换为纳秒 (Convert to nanoseconds)
            svcSleepThread(sleep_ns); // 使用Switch系统调用进行精确休眠 (Use Switch system call for precise sleep)
        }
        
        // 计算实际FPS (Calculate actual FPS) - 在延时后计算以获得准确的帧时间
        // 这用于性能监控和调试 (This is used for performance monitoring and debugging)
        auto current_time = std::chrono::steady_clock::now(); // 获取当前时间戳 (Get current timestamp)
        auto total_frame_time = current_time - last_frame_time; // 计算完整帧时间（包括休眠） (Calculate complete frame time including sleep)
        auto frame_time_us = std::chrono::duration_cast<std::chrono::microseconds>(total_frame_time).count(); // 转换为微秒 (Convert to microseconds)
        if (frame_time_us > 0) { // 避免除零错误 (Avoid division by zero)
            this->FPS = 1000000.0f / static_cast<float>(frame_time_us); // 转换为FPS (Convert to FPS)
        }
        
        last_frame_time = current_time; // 更新上一帧时间戳 (Update last frame timestamp)
    }
}

void Controller::UpdateButtonHeld(bool& down, bool held) { // 更新按钮长按状态的方法 (Method to update button held state)
    // 处理按钮长按的加速重复逻辑 (Handle accelerated repeat logic for button holding)
    if (down) { // 如果按钮刚被按下 (If button was just pressed)
        this->step = 50; // 重置步长为初始值 (Reset step to initial value)
        this->counter = 0; // 重置计数器 (Reset counter)
    } else if (held) { // 如果按钮持续被按住 (If button is continuously held)
        this->counter += this->step; // 增加计数器 (Increment counter)

        // 当计数器达到阈值时触发重复按下事件 (Trigger repeat press event when counter reaches threshold)
        // 这实现了长按时的加速重复功能 (This implements accelerated repeat functionality during long press)
        if (this->counter >= this->MAX) {
            down = true; // 设置按下状态为真 (Set pressed state to true)
            this->counter = 0; // 重置计数器准备下次重复 (Reset counter for next repeat)
            this->step = std::min(this->step + 50, this->MAX_STEP); // 增加步长实现加速，但不超过最大值 (Increase step for acceleration, but not exceeding maximum)
        }
    }
}

void App::Poll() { // 输入事件轮询方法 (Input event polling method)
    // 输入事件处理时间限制：最大3毫秒，防止复杂输入处理影响帧率 (Input processing time limit: max 3ms to prevent complex input handling from affecting frame rate)
    // 这确保了即使在复杂输入处理时也能维持60FPS (This ensures 60FPS is maintained even during complex input processing)
    constexpr auto max_input_time = std::chrono::microseconds(3000); // 3毫秒 (3 milliseconds)
    auto input_start = std::chrono::steady_clock::now(); // 记录输入处理开始时间 (Record input processing start time)
    
    padUpdate(&this->pad); // 更新手柄状态，从Switch系统获取最新输入数据 (Update controller state, get latest input data from Switch system)

    const auto down = padGetButtonsDown(&this->pad); // 获取本帧新按下的按钮 (Get buttons pressed this frame)
    const auto held = padGetButtons(&this->pad); // 获取当前持续按住的按钮 (Get buttons currently held)

    // 检查是否超时，如果超时则跳过复杂的按键处理 (Check for timeout, skip complex key processing if timed out)
    // 这是第一个超时检查点，确保基础输入获取不会超时 (This is the first timeout checkpoint, ensuring basic input acquisition doesn't timeout)
    auto current_time = std::chrono::steady_clock::now(); // 获取当前时间 (Get current time)
    if (current_time - input_start >= max_input_time) {
        return; // 超时保护，直接返回避免影响帧率 (Timeout protection, return directly to avoid affecting frame rate)
    }

    // 基础按钮状态映射 (Basic button state mapping)
    // 使用位运算检测各个按钮的按下状态 (Use bitwise operations to detect button press states)
    this->controller.A = down & HidNpadButton_A; // A按钮状态 (A button state)
    this->controller.B = down & HidNpadButton_B; // B按钮状态 (B button state)
    this->controller.X = down & HidNpadButton_X; // X按钮状态 (X button state)
    this->controller.Y = down & HidNpadButton_Y; // Y按钮状态 (Y button state)
    this->controller.L = down & HidNpadButton_L; // L肩键状态 (L shoulder button state)
    this->controller.R = down & HidNpadButton_R; // R肩键状态 (R shoulder button state)
    this->controller.L2 = down & HidNpadButton_ZL; // ZL扳机键状态 (ZL trigger button state)
    this->controller.R2 = down & HidNpadButton_ZR; // ZR扳机键状态 (ZR trigger button state)
    this->controller.START = down & HidNpadButton_Plus; // +按钮（开始键）状态 (+ button (start key) state)
    this->controller.SELECT = down & HidNpadButton_Minus; // -按钮（选择键）状态 (- button (select key) state)
    this->controller.RIGHT = down & HidNpadButton_AnyRight; // 右方向键状态 (Right directional key state)
    
    // RIGHT+A组合键检测 (RIGHT+A combination key detection)
    // 简单检测：只要两个键都按下就触发，避免重复触发 (Simple detection: trigger when both keys are pressed, avoid repeated triggering)
    bool right_held = held & HidNpadButton_Right; // 右方向键是否被持续按住 (Whether Right key is being held)
    bool a_held = held & HidNpadButton_A; // A键是否被持续按住 (Whether A key is being held)
    
    // 保存上一帧的组合键状态用于避免重复触发 (Save previous frame combo state to avoid repeated triggering)
    static bool prev_combo_triggered = false;
    
    // 组合键触发条件：两个键都按下且上一帧未触发 (Combination key trigger condition: both keys pressed and not triggered in previous frame)
    if (right_held && a_held && !prev_combo_triggered) {
        this->controller.RIGHT_AND_A = true;
        prev_combo_triggered = true; // 标记已触发，避免重复触发 (Mark as triggered to avoid repeated triggering)
    } else if (!right_held || !a_held) {
        // 任一键释放时重置触发状态 (Reset trigger state when any key is released)
        this->controller.RIGHT_AND_A = false;
        prev_combo_triggered = false;
    } else {
        this->controller.RIGHT_AND_A = false;
    }
    
    // 再次检查时间，避免方向键处理超时 (Check time again to avoid directional key processing timeout)
    // 这是第二个超时检查点，确保方向键处理有足够时间 (This is the second timeout checkpoint, ensuring directional key processing has enough time)
    current_time = std::chrono::steady_clock::now(); // 更新当前时间 (Update current time)
    if (current_time - input_start >= max_input_time) {
        return; // 超时保护 (Timeout protection)
    }
    
    // 方向键状态处理 (Directional key state processing)
    // keep directional keys pressed. 保持方向键按下状态 (Keep directional keys pressed)
    this->controller.DOWN = (down & HidNpadButton_AnyDown); // 下方向键状态 (Down directional key state)
    this->controller.UP = (down & HidNpadButton_AnyUp); // 上方向键状态 (Up directional key state)
    this->controller.LEFT = (down & HidNpadButton_AnyLeft); // 左方向键状态 (Left directional key state)
    this->controller.RIGHT = (down & HidNpadButton_AnyRight); // 右方向键状态 (Right directional key state)
    

    // 更新方向键的长按状态 (Update directional key held states)
    // 这些调用处理方向键的加速重复功能 (These calls handle accelerated repeat functionality for directional keys)
    this->controller.UpdateButtonHeld(this->controller.DOWN, held & HidNpadButton_AnyDown); // 更新下键长按状态 (Update down key held state)
    this->controller.UpdateButtonHeld(this->controller.UP, held & HidNpadButton_AnyUp); // 更新上键长按状态 (Update up key held state)
    this->controller.UpdateButtonHeld(this->controller.LEFT, held & HidNpadButton_AnyLeft); // 更新左键长按状态 (Update left key held state)
    this->controller.UpdateButtonHeld(this->controller.RIGHT, held & HidNpadButton_AnyRight); // 更新右键长按状态 (Update right key held state)
    if (this->show_newdialog && this->newdialog_type == newDialogType::SEARCH) {
        this->controller.B = (down & HidNpadButton_B); // B按钮状态 (B button state)
        this->controller.UpdateButtonHeld(this->controller.B, held & HidNpadButton_B); // 更新B按钮长按状态 (Update B button held state)
    }
    

#ifndef NDEBUG // 调试模式编译条件 (Debug mode compilation condition)
    // 调试输出也需要时间检查 (Debug output also needs time checking)
    // 确保调试输出不会影响帧率性能 (Ensure debug output doesn't affect frame rate performance)
    current_time = std::chrono::steady_clock::now(); // 获取当前时间用于调试输出时间检查 (Get current time for debug output time checking)
    if (current_time - input_start < max_input_time) { // 只有在时间限制内才执行调试输出 (Only execute debug output within time limit)
        // Lambda函数：用于简化按键状态的调试输出 (Lambda function: simplify debug output for key states)
        // 这个函数只在按键被按下时才输出日志 (This function only outputs log when key is pressed)
        auto display = [](const char* str, bool key) {
            if (key) { // 如果按键被按下 (If key is pressed)
                LOG("Key %s is Pressed\n", str); // 输出按键按下的调试信息 (Output debug info for key press)
            }
        };

        // 调试输出各个按键的状态 (Debug output for each key state)
        // 这些调用帮助开发者了解当前的输入状态 (These calls help developers understand current input state)
        display("A", this->controller.A); // 输出A键状态 (Output A key state)
        display("B", this->controller.B); // 输出B键状态 (Output B key state)
        display("X", this->controller.X); // 输出X键状态 (Output X key state)
        display("Y", this->controller.Y); // 输出Y键状态 (Output Y key state)
        display("L", this->controller.L); // 输出L肩键状态 (Output L shoulder key state)
        display("R", this->controller.R); // 输出R肩键状态 (Output R shoulder key state)
        display("L2", this->controller.L2); // 输出ZL扳机键状态 (Output ZL trigger key state)
        display("R2", this->controller.R2); // 输出ZR扳机键状态 (Output ZR trigger key state)
    }
#endif // 结束调试模式条件编译 (End debug mode conditional compilation)
} // App::Poll()方法结束 (End of App::Poll() method)

// App::Update() - 应用程序主更新方法 (Main application update method)
// 每帧调用一次，处理应用程序的核心逻辑更新 (Called once per frame to handle core application logic updates)
void App::Update() {
    // 每帧处理资源加载（如果启用） (Process resource loading per frame if enabled)
    // 这个机制用于分散资源加载的CPU负担，避免单帧卡顿 (This mechanism distributes CPU load of resource loading to avoid single frame stuttering)
    if (enable_frame_load_limit) { // 检查是否启用了帧限制资源加载 (Check if frame-limited resource loading is enabled)
        resource_manager.processFrameLoads(); // 处理当前帧允许的资源加载任务 (Process resource loading tasks allowed for current frame)
    }
    
    // 处理对话框更新 (Handle dialog update)
    if (this->show_newdialog) {
        this->newUpdateDialog();
        return;
    }
    
    // 根据当前菜单模式执行相应的更新逻辑 (Execute corresponding update logic based on current menu mode)
    // 使用状态机模式管理不同的应用程序状态 (Use state machine pattern to manage different application states)
    switch (this->menu_mode) {
        case MenuMode::LOAD: // 加载模式：应用程序启动时的初始化状态 (Load mode: initialization state during app startup)
            this->UpdateLoad(); // 处理应用程序加载逻辑 (Handle application loading logic)
            break;
        case MenuMode::LIST: // 列表模式：显示应用程序列表的主界面 (List mode: main interface showing application list)
            this->UpdateList(); // 处理应用程序列表的交互和显示 (Handle application list interaction and display)
            // 确保当前屏幕图标始终加载，即使用户不移动光标 (Ensure current screen icons are always loaded, even if user doesn't move cursor)
            this->LoadVisibleAreaIcons();
            break;
        case MenuMode::MODLIST: 
            this->UpdateModList(); 
            break;
        case MenuMode::INSTRUCTION:
            this->UpdateInstruction(); 
            break;
        case MenuMode::ADDGAMELIST:
            this->UpdateAddGameList();
            // 加载可见区域的图标 (Load icons for visible area)
            this->LoadAddGameVisibleAreaIcons();
            break;
        case MenuMode::MTP:
            this->UpdateMTP();
            break;

    }
} // App::Update()方法结束 (End of App::Update() method)

// App::Draw() - 应用程序主渲染方法 (Main application rendering method)
// 每帧调用一次，负责所有的GPU渲染操作 (Called once per frame, responsible for all GPU rendering operations)
void App::Draw() {
    // GPU命令优化：准备下一个命令缓冲区 (GPU command optimization: Prepare next command buffer)
    // GPU command optimization: Prepare next command buffer
    // 双缓冲机制，避免GPU等待CPU准备命令 (Double buffering mechanism to avoid GPU waiting for CPU command preparation)
    this->prepareNextCommandBuffer();
    
    // 从交换链获取可用的图像槽位 (Acquire available image slot from swapchain)
    // 这是Vulkan/deko3d渲染管线的标准流程 (This is standard Vulkan/deko3d rendering pipeline procedure)
    const auto slot = this->queue.acquireImage(this->swapchain);
    
    // 提交静态命令（帧缓冲区绑定和基础渲染状态） (Submit static commands - framebuffer binding and basic render state)
    // Submit static commands (framebuffer binding and basic render state)
    // 静态命令在应用启动时预先录制，提高渲染效率 (Static commands are pre-recorded at app startup for rendering efficiency)
    this->queue.submitCommands(this->framebuffer_cmdlists[slot]); // 提交帧缓冲区绑定命令 (Submit framebuffer binding commands)
    this->queue.submitCommands(this->render_cmdlist); // 提交基础渲染状态命令 (Submit basic render state commands)
    
    // 开始记录动态命令到当前命令缓冲区 (Start recording dynamic commands to current command buffer)
    // Start recording dynamic commands to current command buffer
    // 动态命令每帧重新录制，包含实际的绘制调用 (Dynamic commands are re-recorded each frame, containing actual draw calls)
    auto& current_cmdbuf = this->dynamic_cmdbufs[this->current_cmdbuf_index]; // 获取当前帧的命令缓冲区 (Get command buffer for current frame)
    current_cmdbuf.clear(); // 清空上一帧的命令 (Clear commands from previous frame)
    
    // NanoVG渲染命令 (NanoVG rendering commands)
    // NanoVG rendering commands
    // 开始NanoVG帧渲染，设置屏幕尺寸和像素比 (Begin NanoVG frame rendering with screen size and pixel ratio)
    nvgBeginFrame(this->vg, SCREEN_WIDTH, SCREEN_HEIGHT, 1.f);
    
    // 绘制背景元素（标题栏、分割线等） (Draw background elements - title bar, dividers, etc.)
    this->DrawBackground();
    
    // 根据当前菜单模式绘制相应的界面内容 (Draw corresponding interface content based on current menu mode)
    // 使用状态机模式管理不同界面的渲染逻辑 (Use state machine pattern to manage rendering logic for different interfaces)
    switch (this->menu_mode) {
        case MenuMode::LOAD: // 加载界面：显示启动画面和初始化进度 (Load interface: show startup screen and initialization progress)
            this->DrawLoad(); // 绘制加载界面 (Draw loading interface)
            break;
        case MenuMode::LIST: // 列表界面：显示应用程序列表和选择器 (List interface: show application list and selector)
            this->DrawList(); // 绘制应用程序列表 (Draw application list)
            break;
        case MenuMode::MODLIST: 
            this->DrawModList(); 
            break;
        case MenuMode::INSTRUCTION:
            this->DrawInstruction(); 
            break;
        case MenuMode::ADDGAMELIST:
            this->DrawAddGameList();
            break;
        case MenuMode::MTP:
            this->DrawMTP();
            break;

    }

    this->newDrawDialog();

    // 结束NanoVG帧渲染，提交所有绘制命令到GPU (End NanoVG frame rendering, submit all draw commands to GPU)
    nvgEndFrame(this->vg);
    
    // 完成动态命令列表记录 (Finish dynamic command list recording)
    // Finish dynamic command list recording
    // 将录制的命令转换为可执行的命令列表 (Convert recorded commands to executable command list)
    this->dynamic_cmdlists[this->current_cmdbuf_index] = current_cmdbuf.finishList();
    
    // 异步提交动态命令（不阻塞CPU） (Asynchronously submit dynamic commands - non-blocking CPU)
    // Asynchronously submit dynamic commands (non-blocking CPU)
    // 允许CPU继续处理下一帧，提高整体性能 (Allow CPU to continue processing next frame for better overall performance)
    this->submitCurrentCommandBuffer();
    
    // 呈现图像 (Present image)
    // Present image
    // 将渲染结果显示到屏幕上 (Display rendering result to screen)
    this->queue.presentImage(this->swapchain, slot);
} // App::Draw()方法结束 (End of App::Draw() method)

// App::DrawBackground() - 绘制应用程序背景界面 (Draw application background interface)
// 包含背景色、分割线、标题和版本信息 (Includes background color, dividers, title and version info)
void App::DrawBackground() {
    
    // 绘制全屏黑色背景 (Draw full-screen black background)
    // 作为所有UI元素的基础背景层 (Serves as base background layer for all UI elements)
    gfx::drawRect(this->vg, 0.f, 0.f, SCREEN_WIDTH, SCREEN_HEIGHT, gfx::Colour::BLACK);
    
    // 绘制顶部分割线 (Draw top divider line)
    // 分离标题区域和主内容区域 (Separate title area from main content area)
    gfx::drawRect(vg, 30.f, 86.0f, 1220.f, 1.f, gfx::Colour::WHITE);
    
    // 绘制底部分割线 (Draw bottom divider line)
    // 分离主内容区域和底部状态栏 (Separate main content area from bottom status bar)
    gfx::drawRect(vg, 30.f, 646.0f, 1220.f, 1.f, gfx::Colour::WHITE);
    
// 版本字符串宏定义 (Version string macro definitions)
// uses the APP_VERSION define in makefile for string version.
// source: https://stackoverflow.com/a/2411008
// 将宏值转换为字符串的标准C预处理器技巧 (Standard C preprocessor trick to convert macro values to strings)
#define STRINGIZE(x) #x // 第一层宏：将参数转换为字符串字面量 (First-level macro: convert parameter to string literal)
#define STRINGIZE_VALUE_OF(x) STRINGIZE(x) // 第二层宏：先展开宏值再转换为字符串 (Second-level macro: expand macro value then convert to string)
    // 在右上角显示应用程序版本号 (Display application version number in top-right corner)
    // 使用银色字体，较小尺寸，不干扰主要内容 (Use silver font, smaller size, doesn't interfere with main content)
    gfx::drawText(this->vg, 70.f, 40.f, 28.f, SOFTWARE_TITLE.c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE);
    
    gfx::drawText(this->vg, 1224.f, 45.f, 22.f, STRINGIZE_VALUE_OF(UNTITLED_VERSION_STRING), nullptr, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP, gfx::Colour::SILVER);
    
    
// 清理宏定义，避免污染全局命名空间 (Clean up macro definitions to avoid polluting global namespace)
#undef STRINGIZE
#undef STRINGIZE_VALUE_OF

} // App::DrawBackground()方法结束 (End of App::DrawBackground() method)

// App::DrawLoad() - 绘制加载界面 (Draw loading interface)
// 显示应用程序扫描进度和加载状态信息 (Display application scanning progress and loading status information)
void App::DrawLoad() {
    
    // 在屏幕中央显示加载文本 (Display loading text in screen center)
    // 使用黄色突出显示，36px字体大小，居中对齐 (Use yellow highlight, 36px font size, center alignment)
    // 位置稍微上移40像素，为底部按钮留出空间 (Position slightly moved up 40 pixels to leave space for bottom buttons)
    gfx::drawTextArgs(this->vg, SCREEN_WIDTH / 2.f, SCREEN_HEIGHT / 2.f - 40.f, 36.f, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, gfx::Colour::YELLOW, "");
    
    // 在底部显示返回按钮提示 (Display back button hint at bottom)
    // 白色文字，显示B键对应的返回操作 (White text, showing B key corresponding to back operation)
    // 为用户提供退出加载界面的选项 (Provide user with option to exit loading interface)
    gfx::drawButtons(this->vg, gfx::Colour::WHITE, gfx::pair{gfx::Button::B, BUTTON_EXIT.c_str()});
} // App::DrawLoad()方法结束 (End of App::DrawLoad() method)



// App::DrawList() - 绘制应用程序九宫格界面 (Draw application grid interface)
// 显示可删除的应用程序九宫格，包含图标、标题、大小信息和选择状态 (Display deletable application grid with icons, titles, size info and selection status)
void App::DrawList() {

    SOFTWARE_TITLE = SOFTWARE_TITLE_APP;


    std::scoped_lock lock{entries_mutex}; // 保护entries向量的读取操作 (Protect entries vector read operations)
    // 如果没有应用，显示加载提示 (If no apps, show loading hint)
    // If no apps, show loading hint
    // 处理应用列表为空的边界情况 (Handle edge case when application list is empty)
    if (this->entries.empty()) {
        // 在屏幕中央显示加载提示文本 (Display loading hint text in screen center)
        gfx::drawTextBoxCentered(this->vg, 0.f, 0.f, 1280.f, 720.f, 35.f, 1.5f, NO_FOUND_MOD.c_str(), nullptr, gfx::Colour::SILVER);
        gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_EXIT.c_str()},
            gfx::pair{gfx::Button::X, Add_Mod_BUTTON.c_str()},
            gfx::pair{gfx::Button::PLUS, INSTRUCTION_BUTTON.c_str()});

        return; // 提前返回，不执行后续的列表绘制逻辑 (Early return, skip subsequent list drawing logic)
    }
    
    

    // 铺满界面的长方形布局常量定义 (Full-screen rectangular layout constant definitions)
    // 避开标题区域(86px以上)和按钮区域(670px以下) (Avoid title area above 86px and button area below 670px)
    
    // 布局参数 (Layout parameters) - 每页显示9个项目
    constexpr auto grid_cols = 3;           // 每行显示3个项目 (3 items per row)
    constexpr auto grid_rows = 3;           // 每页显示3行 (3 rows per page)
    constexpr auto items_per_page = grid_cols * grid_rows; // 每页显示9个项目 (9 items per page)
    
    // 可用屏幕区域 (Available screen area)
    constexpr auto available_top = 106.f;    // 距离标题分界线20像素 (20 pixels from title divider line)
    constexpr auto available_bottom = 626.f; // 距离按钮分界线20像素 (20 pixels from button divider line)
    constexpr auto available_left = 50.f;    // 左边距 (Left margin) - 增加20.f
    constexpr auto available_right = 1230.f; // 右边距 (Right margin) - 减少20.f
    constexpr auto available_width = available_right - available_left;   // 可用宽度 (Available width)
    constexpr auto available_height = available_bottom - available_top;  // 可用高度 (Available height)
    
    // 长方形项目尺寸 (Rectangular item dimensions) - 铺满可用区域
    constexpr auto item_spacing = 15.f;     // 项目间距 (Item spacing) - 增加间距
    constexpr auto item_width = (available_width - (grid_cols - 1) * item_spacing) / grid_cols;  // 每个格子的宽度 (Width of each cell)
    constexpr auto item_height = (available_height - (grid_rows - 1) * item_spacing) / grid_rows; // 每个格子的高度 (Height of each cell)
    
    // 格子内部布局 (Internal cell layout)
    constexpr auto icon_size = 120.f;        // 图标大小 (Icon size)
    constexpr auto icon_margin = 20.f;      // 图标边距 (Icon margin) - 增加边距
    constexpr auto text_margin_left = 15.f; // 文本左边距 (Text left margin) - 增加边距
    constexpr auto text_line_height = 28.f; // 文本行高 (Text line height) - 增加行间距

    // 保存当前绘图状态并设置裁剪区域 (Save current drawing state and set clipping area)
    nvgSave(this->vg);
    nvgScissor(this->vg, 30.f, 86.0f, 1220.f, 646.0f); // 裁剪区域 (Clipping area)

    // 九宫格布局不需要固定的Y坐标偏移量 (Grid layout doesn't need fixed Y coordinate offset)
    // Y坐标将在九宫格绘制循环中动态计算 (Y coordinate will be calculated dynamically in grid drawing loop)

    // 遍历并绘制应用列表项 (Iterate and draw application list items)
    // 计算当前页的起始索引 (Calculate start index for current page)
    const size_t page_start = (this->index / items_per_page) * items_per_page;
    const size_t page_end = std::min(page_start + items_per_page, this->entries.size());
    
    // 长方形格子绘制循环 (Rectangular cell drawing loop)
    for (size_t i = page_start; i < page_end; i++) {
        // 计算当前项在网格中的位置 (Calculate current item position in grid)
        const size_t grid_index = i - page_start;
        const size_t col = grid_index % grid_cols;
        const size_t row = grid_index / grid_cols;
        
        // 计算长方形格子的绘制坐标 (Calculate rectangular cell drawing coordinates)
        const float item_x = available_left + col * (item_width + item_spacing);
        const float item_y = available_top + row * (item_height + item_spacing);
        
        // 检查是否为当前光标选中的项 (Check if this is the currently cursor-selected item)
        if (i == this->index) {
            // 当前选中项：绘制彩色边框和半透明背景 (Current selected item: draw colored border and semi-transparent background)
            auto col = pulse.col;  // 获取脉冲颜色 (Get pulse color)
            col.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
            col.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
            col.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
            col.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
            update_pulse_colour(); // 更新脉冲颜色(产生闪烁效果) (Update pulse color for blinking effect)
            
            // 绘制选中项的彩色边框 (Draw colored border for selected item)
            gfx::drawRect(this->vg, item_x - 5.f, item_y - 5.f, item_width + 10.f, item_height + 10.f, col);
            // 绘制选中项的半透明背景 (Draw semi-transparent background for selected item)
            NVGcolor bg_col = {45.f/255.f, 45.f/255.f, 45.f/255.f, 1.0f}; // 黑色半透明背景 (Black semi-transparent background)
            gfx::drawRect(this->vg, item_x, item_y, item_width, item_height, bg_col);
        }
        
        if (i == this->index) {
            // 绘制选中格子的淡蓝色透明蒙版背景 (Draw light blue transparent mask background for selected cell)
            NVGcolor cell_bg_col = {0.7f, 0.85f, 1.0f, 0.08f}; // 淡蓝色透明蒙版 (Light blue transparent mask)
            gfx::drawRect(this->vg, item_x, item_y, item_width, item_height, cell_bg_col);
        }
        else {
            // 绘制格子的透明蒙版背景 (Draw transparent mask background for cell)
            NVGcolor cell_bg_col = {0.f, 0.f, 0.f, 0.08f}; // 黑色透明蒙版，降低透明度使颜色更淡 (Black transparent mask with lower opacity for lighter color)
            gfx::drawRect(this->vg, item_x, item_y, item_width, item_height, cell_bg_col);
        }
        
        // 左侧图标区域 (Left icon area)
        const float icon_x = item_x + icon_margin;
        const float icon_y = item_y + (item_height - icon_size) / 2.f; // 图标垂直居中 (Icon vertically centered)
        
        // 创建并绘制应用图标 (Create and draw application icon)
        const auto icon_paint = nvgImagePattern(this->vg, icon_x, icon_y, icon_size, icon_size, 0.f, this->entries[i].image, 1.f);
        // 使用圆角矩形绘制图标 (Draw icon with rounded corners)
        nvgBeginPath(this->vg);
        nvgRoundedRect(this->vg, icon_x, icon_y, icon_size, icon_size, 5.f); // 5像素圆角 (5 pixel corner radius)
        nvgFillPaint(this->vg, icon_paint);
        nvgFill(this->vg);
        
        bool is_favorite = entries[i].is_favorite;
        if (is_favorite){
            // 绘制爱心图标在游戏图标右上角 (Draw heart icon at top-right corner of game icon)
            const float heart_size = 25.f; // 爱心图标大小 (Heart icon size)
            const float heart_margin = 5.f; // 边距 (Margin)
            const float heart_x = icon_x + icon_size - heart_size - heart_margin; // 爱心X坐标 (Heart X coordinate)
            const float heart_y = icon_y + heart_margin; // 爱心Y坐标 (Heart Y coordinate)
            
            
            // 创建并绘制爱心图标 (Create and draw heart icon)
            const auto heart_paint = nvgImagePattern(this->vg, heart_x, heart_y, heart_size, heart_size, 0.f, this->like_image, 1.f);
            nvgBeginPath(this->vg);
            nvgRect(this->vg, heart_x, heart_y, heart_size, heart_size);
            nvgFillPaint(this->vg, heart_paint);
            nvgFill(this->vg);
        }
        
        // 右侧文本区域 (Right text area)
        const float text_start_x = icon_x + icon_size + text_margin_left;
        const float text_start_y = icon_y + 6.f; // 文本起始位置与图标顶边对齐 (Text start position aligned with icon top edge)
        const float text_width = item_width - (text_start_x - item_x) - 20.f; // 文本区域宽度 (Text area width)
        
        
        // 保存当前绘图状态并设置文本裁剪区域 (Save current drawing state and set text clipping area)
        nvgSave(this->vg);
        nvgScissor(this->vg, text_start_x, item_y + 5.f, text_width, item_height - 10.f);
        
        // 在这里声明一个颜色，根据是否收藏来设置颜色
        gfx::Colour text_title_col = is_favorite ? gfx::Colour::TEAL : gfx::Colour::WHITE;
        gfx::Colour text_context_col = is_favorite ? gfx::Colour::TEAL : gfx::Colour::SILVER;

        // 绘制应用名称 (Draw application name) - 第一行 - 标题字体23
        gfx::drawText(this->vg, text_start_x, text_start_y, 23.f, this->entries[i].FILE_NAME2.c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, text_title_col); 
        
        // 绘制游戏版本号 (Draw game version) - 第二行 - 其他内容字体19
        gfx::drawText(this->vg, text_start_x, text_start_y + text_line_height + 5.f, 19.f, 
                     (Game_VERSION_TAG + this->entries[i].display_version ).c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, text_context_col);
        
        // 绘制模组版本号 (Draw mod version) - 第三行 - 其他内容字体19
        gfx::drawText(this->vg, text_start_x, text_start_y + text_line_height * 2 + 5.f, 19.f, 
                     (MOD_VERSION_TAG + this->entries[i].MOD_VERSION).c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, text_context_col);

        // 绘制模组数量 (Draw mod total) - 第四行 - 其他内容字体19
        gfx::drawText(this->vg, text_start_x, text_start_y + text_line_height * 3 + 5.f, 19.f, 
                     (MOD_COUNT_TAG + this->entries[i].MOD_TOTAL).c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, text_context_col);
        
        // 恢复之前保存的绘图状态 (Restore previously saved drawing state)
        nvgRestore(this->vg);
    }

    
    gfx::drawTextArgs(this->vg, 55.f, 670.f, 24.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE, "%zu / %zu", this->index + 1, this->entries.size());

    // 恢复NanoVG绘图状态 / Restore NanoVG drawing state
    nvgRestore(this->vg);

    
    
    
    
    // 检查扫描状态，动态调整按钮颜色 / Check scan status and dynamically adjust button colors
    gfx::Colour y_plus_color = this->is_scan_running ? gfx::Colour::GREY : gfx::Colour::WHITE;
    
    // 使用 drawButtons2Colored 函数简化按钮绘制 / Use drawButtons2Colored function to simplify button drawing
    gfx::drawButtons2Colored(this->vg,
        gfx::make_pair2_colored(gfx::Button::B, BUTTON_EXIT.c_str(), gfx::Colour::WHITE),
        gfx::make_pair2_colored(gfx::Button::A, BUTTON_SELECT.c_str(), gfx::Colour::WHITE),
        gfx::make_pair2_colored(gfx::Button::X, ABOUT_BUTTON.c_str(), gfx::Colour::WHITE),
        gfx::make_pair2_colored(gfx::Button::Y, this->GetSortStr(), y_plus_color),
        gfx::make_pair2_colored(gfx::Button::PLUS, INSTRUCTION_BUTTON.c_str(), gfx::Colour::WHITE),
        gfx::make_pair2_colored(gfx::Button::MINUS, SEARCH_BUTTON.c_str(), y_plus_color)
    );

}


/**
 * @brief 获取所有应用程序ID
 * @param app_ids 存储应用程序ID的向量引用
 * @return 成功返回0，失败返回错误码
 * 
 * 此函数通过分页调用nsListApplicationRecord获取所有应用程序ID，
 * 支持任意数量的应用，不受缓冲区大小限制。
 */
Result App::GetAllApplicationIds(std::vector<u64>& app_ids) {
    app_ids.clear();

    // 定义每页获取的应用记录数量
    constexpr size_t PAGE_SIZE = 30;
    std::array<NsApplicationRecord, PAGE_SIZE> record_list;
    s32 record_count = 0;
    s32 offset = 0;
    Result result = 0;

    // 分页获取应用记录，直到获取不到新记录为止
    do {
        // 调用nsListApplicationRecord获取一页应用记录
        result = nsListApplicationRecord(record_list.data(), static_cast<s32>(record_list.size()), offset, &record_count);

        // 检查是否获取成功
        if (R_FAILED(result)) {
            return result;
        }

        // 如果没有获取到记录，退出循环
        if (record_count <= 0) {
            break;
        }

        // 确保记录数量不超过缓冲区大小
        record_count = std::min(record_count, static_cast<s32>(record_list.size()));

        // 将获取到的应用ID添加到向量中，只收集以0100开头的游戏ID
        // Add obtained application IDs to vector, only collect game IDs starting with 0100
        for (s32 i = 0; i < record_count; i++) {
            u64 app_id = record_list[i].application_id;
            // 检查应用ID是否为游戏ID格式（高32位以01开头）
            // Check if application ID is game ID format (high 32 bits start with 01)
            u32 high_part = (u32)(app_id >> 32);
            if ((high_part & 0xFF000000) == 0x01000000) {
                app_ids.push_back(app_id);
            }
        }

        // 更新偏移量，准备获取下一页
        offset += record_count;
    } while (record_count > 0);

    return 0;
}



void App::DrawAddGameList() {

    SOFTWARE_TITLE = SOFTWARE_TITLE_ADDGAME;

    std::scoped_lock lock{entries_AddGame_mutex}; 

    // 如果没有游戏，显示加载提示 (If no games, show loading hint)
    if (this->entries_AddGame.empty() && addgame_scan_running.load()) {
        // 在屏幕中央显示加载提示文本 (Display loading hint text in screen center)
        gfx::drawTextBoxCentered(this->vg, 0.f, 0.f, 1280.f, 720.f, 35.f, 1.5f, LOADING_TEXT.c_str(), nullptr, gfx::Colour::SILVER);
        gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_EXIT.c_str()}
            );

    } else if (this->entries_AddGame.empty() && !addgame_scan_running.load()) {
        // 在屏幕中央显示加载提示文本 (Display loading hint text in screen center)
        gfx::drawTextBoxCentered(this->vg, 0.f, 0.f, 1280.f, 720.f, 35.f, 1.5f, NO_FOUND_GAME.c_str(), nullptr, gfx::Colour::SILVER);
        gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_BACK.c_str()}
            );
        return; // 提前返回，不执行后续的列表绘制逻辑 (Early return, skip subsequent list drawing logic)
    } 

    // 检查扫描状态，动态调整按钮颜色 / Check scan status and dynamically adjust button colors
    gfx::Colour y_button_color = addgame_scan_running.load() ? gfx::Colour::GREY : gfx::Colour::WHITE;
    
    // 使用 drawButtons2Colored 函数简化按钮绘制 / Use drawButtons2Colored function to simplify button drawing
    gfx::drawButtons2Colored(this->vg,
        gfx::make_pair2_colored(gfx::Button::B, BUTTON_BACK.c_str(), gfx::Colour::WHITE),
        gfx::make_pair2_colored(gfx::Button::A, BUTTON_SELECT.c_str(), gfx::Colour::WHITE),
        gfx::make_pair2_colored(gfx::Button::Y, this->GetSortStr2(), y_button_color),
        gfx::make_pair2_colored(gfx::Button::MINUS, SEARCH_BUTTON.c_str(), y_button_color)
    );
    
    // 九宫格布局常量定义 (Grid layout constant definitions)
    // 布局参数 (Layout parameters) - 每页显示9个项目
    constexpr auto grid_cols = 3;           // 每行显示3个项目 (3 items per row)
    constexpr auto grid_rows = 3;           // 每页显示3行 (3 rows per page)
    constexpr auto items_per_page = grid_cols * grid_rows; // 每页显示9个项目 (9 items per page)
    
    // 可用屏幕区域 (Available screen area)
    constexpr auto available_top = 106.f;    // 距离标题分界线20像素 (20 pixels from title divider line)
    constexpr auto available_bottom = 626.f; // 距离按钮分界线20像素 (20 pixels from button divider line)
    constexpr auto available_left = 50.f;    // 左边距 (Left margin)
    constexpr auto available_right = 1230.f; // 右边距 (Right margin)
    constexpr auto available_width = available_right - available_left;   // 可用宽度 (Available width)
    constexpr auto available_height = available_bottom - available_top;  // 可用高度 (Available height)
    
    // 长方形项目尺寸 (Rectangular item dimensions) - 铺满可用区域
    constexpr auto item_spacing = 15.f;     // 项目间距 (Item spacing)
    constexpr auto item_width = (available_width - (grid_cols - 1) * item_spacing) / grid_cols;  // 每个格子的宽度 (Width of each cell)
    constexpr auto item_height = (available_height - (grid_rows - 1) * item_spacing) / grid_rows; // 每个格子的高度 (Height of each cell)
    
    // 格子内部布局 (Internal cell layout)
    constexpr auto icon_size = 120.f;        // 图标大小 (Icon size)
    constexpr auto icon_margin = 20.f;      // 图标边距 (Icon margin)
    constexpr auto text_margin_left = 15.f; // 文本左边距 (Text left margin)
    constexpr auto text_line_height = 28.f * 2; // 文本行高 (Text line height)

    // 保存当前绘图状态并设置裁剪区域 (Save current drawing state and set clipping area)
    nvgSave(this->vg);
    nvgScissor(this->vg, 30.f, 86.0f, 1220.f, 646.0f); // 裁剪区域 (Clipping area)

    // 遍历并绘制游戏列表项 (Iterate and draw game list items)
    // 计算当前页的起始索引 (Calculate start index for current page)
    const size_t page_start = (this->index_AddGame / items_per_page) * items_per_page;
    const size_t page_end = std::min(page_start + items_per_page, this->entries_AddGame.size());
    
    // 九宫格绘制循环 (Grid drawing loop)
    for (size_t i = page_start; i < page_end; i++) {
        // 计算当前项在网格中的位置 (Calculate current item position in grid)
        const size_t grid_index = i - page_start;
        const size_t col = grid_index % grid_cols;
        const size_t row = grid_index / grid_cols;
        
        // 计算长方形格子的绘制坐标 (Calculate rectangular cell drawing coordinates)
        const float item_x = available_left + col * (item_width + item_spacing);
        const float item_y = available_top + row * (item_height + item_spacing);
        
        // 检查是否为当前光标选中的项 (Check if this is the currently cursor-selected item)
        if (i == this->index_AddGame) {
            // 当前选中项：绘制彩色边框和半透明背景 (Current selected item: draw colored border and semi-transparent background)
            auto col = pulse.col;  // 获取脉冲颜色 (Get pulse color)
            col.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
            col.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
            col.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
            col.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
            update_pulse_colour(); // 更新脉冲颜色(产生闪烁效果) (Update pulse color for blinking effect)
            
            // 绘制选中项的彩色边框 (Draw colored border for selected item)
            gfx::drawRect(this->vg, item_x - 5.f, item_y - 5.f, item_width + 10.f, item_height + 10.f, col);
            // 绘制选中项的半透明背景 (Draw semi-transparent background for selected item)
            NVGcolor bg_col = {45.f/255.f, 45.f/255.f, 45.f/255.f, 1.0f}; // 黑色半透明背景 (Black semi-transparent background)
            gfx::drawRect(this->vg, item_x, item_y, item_width, item_height, bg_col);
        }
        
        if (i == this->index_AddGame) {
            // 绘制选中格子的淡蓝色透明蒙版背景 (Draw light blue transparent mask background for selected cell)
            NVGcolor cell_bg_col = {0.7f, 0.85f, 1.0f, 0.08f}; // 淡蓝色透明蒙版 (Light blue transparent mask)
            gfx::drawRect(this->vg, item_x, item_y, item_width, item_height, cell_bg_col);
        }
        else {
            // 绘制格子的透明蒙版背景 (Draw transparent mask background for cell)
            NVGcolor cell_bg_col = {0.f, 0.f, 0.f, 0.08f}; // 黑色透明蒙版，降低透明度使颜色更淡 (Black transparent mask with lower opacity for lighter color)
            gfx::drawRect(this->vg, item_x, item_y, item_width, item_height, cell_bg_col);
        }
        
        // 左侧图标区域 (Left icon area)
        const float icon_x = item_x + icon_margin;
        const float icon_y = item_y + (item_height - icon_size) / 2.f; // 图标垂直居中 (Icon vertically centered)
        
        // 创建并绘制游戏图标 (Create and draw game icon)
        const auto icon_paint = nvgImagePattern(this->vg, icon_x, icon_y, icon_size, icon_size, 0.f, this->entries_AddGame[i].image, 1.f);
        // 使用圆角矩形绘制图标 (Draw icon with rounded corners)
        nvgBeginPath(this->vg);
        nvgRoundedRect(this->vg, icon_x, icon_y, icon_size, icon_size, 5.f); // 5像素圆角 (5 pixel corner radius)
        nvgFillPaint(this->vg, icon_paint);
        nvgFill(this->vg);
        
        // 右侧文本区域 (Right text area)
        const float text_start_x = icon_x + icon_size + text_margin_left;
        const float text_start_y = icon_y + 6.f; // 文本起始位置与图标顶边对齐 (Text start position aligned with icon top edge)
        const float text_width = item_width - (text_start_x - item_x) - 20.f; // 文本区域宽度 (Text area width)
        
        // 保存当前绘图状态并设置文本裁剪区域 (Save current drawing state and set text clipping area)
        nvgSave(this->vg);
        nvgScissor(this->vg, text_start_x, item_y + 5.f, text_width, item_height - 10.f);
        
        // 绘制游戏名称 (Draw game name) - 第一行 - 标题字体23
        gfx::drawText(this->vg, text_start_x, text_start_y, 23.f, this->entries_AddGame[i].name.c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::TEAL);
        
        // 绘制游戏版本号 (Draw game version) - 第二行 - 其他内容字体19
        if (!this->entries_AddGame[i].display_version.empty()) {
            gfx::drawText(this->vg, text_start_x, text_start_y + text_line_height + 5.f, 19.f, 
                         (VERSION_TAG + this->entries_AddGame[i].display_version).c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::TEAL);
        }
        
        // 恢复之前保存的绘图状态 (Restore previously saved drawing state)
        nvgRestore(this->vg);
    }

    // 显示当前页面信息 (Display current page info)
    gfx::drawTextArgs(this->vg, 55.f, 670.f, 24.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE, "%zu / %zu", this->index_AddGame + 1, this->entries_AddGame.size());

    // 恢复NanoVG绘图状态 (Restore NanoVG drawing state)
    nvgRestore(this->vg);
    
    
}


void App::DrawModList() {
    std::scoped_lock lock{entries_mutex}; // 保护entries向量的读取操作 (Protect entries vector read operations)


    // 如果当前游戏没有MOD，显示提示信息 (If current game has no MODs, show hint)
    if (mod_info.empty()) {
        // 在屏幕中央显示加载提示文本 (Display loading hint text in screen center)
        gfx::drawTextBoxCentered(this->vg, 0.f, 0.f, 1280.f, 720.f, 35.f, 1.5f, NO_FOUND_MOD.c_str(), nullptr, gfx::Colour::SILVER);
        gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_BACK.c_str()});
        return; // 提前返回，不执行后续的列表绘制逻辑 (Early return, skip subsequent list drawing logic)
    }

    // UI布局常量定义 (UI layout constant definitions)
    // 这些常量定义了列表项和侧边栏的精确布局参数 (These constants define precise layout parameters for list items and sidebar)
    
    // 定义列表项的高度 (120像素) (Define list item height - 120 pixels)
    // 每个应用条目的垂直空间 (Vertical space for each application entry)
    constexpr auto box_height = 120.f;
    
    // 定义列表项的宽度 (715像素) (Define list item width - 715 pixels)
    // 左侧应用列表区域的水平空间 (Horizontal space for left application list area)
    constexpr auto box_width = 715.f;
    
    // 定义图标与边框的间距 (12像素) (Define spacing between icon and border - 12 pixels)
    // 应用图标距离列表项边框的内边距 (Inner padding of app icon from list item border)
    constexpr auto icon_spacing = 15.f;
    
    // 定义标题距离左侧的间距 (116像素) (Define title spacing from left - 116 pixels)
    // 应用标题文本的左侧起始位置 (Left starting position of application title text)
    constexpr auto title_spacing_left = 116.f;
    
    // 定义标题距离顶部的间距 (30像素) (Define title spacing from top - 30 pixels)
    // 应用标题文本的垂直位置 (Vertical position of application title text)
    constexpr auto title_spacing_top = 30.f;
    
    // 定义文本距离左侧的间距 (与标题左侧间距相同) (Define text spacing from left - same as title left spacing)
    // 应用详细信息文本的左侧对齐位置 (Left alignment position for application detail text)
    constexpr auto text_spacing_left = title_spacing_left;
    
    // 定义文本距离顶部的间距 (67像素) (Define text spacing from top - 67 pixels)
    // 应用详细信息文本的垂直位置 (Vertical position of application detail text)
    constexpr auto text_spacing_top = 67.f;
    
    // 定义右侧信息框的X坐标 (870像素) (Define right info box X coordinate - 870 pixels)
    // 右侧信息面板的水平起始位置 (Horizontal starting position of right info panel)
    constexpr auto sidebox_x = 870.f;
    
    // 定义右侧信息框的Y坐标 (87像素) (Define right info box Y coordinate - 87 pixels)
    // 右侧信息面板的垂直起始位置 (Vertical starting position of right info panel)
    constexpr auto sidebox_y = 87.f;
    
    // 定义右侧信息框的宽度 (380像素) (Define right info box width - 380 pixels)
    // 右侧信息面板的水平尺寸 (Horizontal dimension of right info panel)
    constexpr auto sidebox_w = 380.f;
    
    // 定义右侧信息框的高度 (558像素) (Define right info box height - 558 pixels)
    // 右侧信息面板的垂直尺寸 (Vertical dimension of right info panel)
    constexpr auto sidebox_h = 558.f;
    
    // 绘制右侧信息框背景 (Draw right info box background)
    // 注：此处原本尝试使用线性渐变，但未实现 (Note: Originally tried to use linear gradient, but not implemented)
    gfx::drawRect(this->vg, sidebox_x, sidebox_y, sidebox_w, sidebox_h, gfx::Colour::LIGHT_BLACK);

    // 保存当前绘图状态并设置裁剪区域 (Save current drawing state and set clipping area)
    nvgSave(this->vg);
    nvgScissor(this->vg, 30.f, 86.0f, 1220.f, 646.0f); // 裁剪区域 (Clipping area)

    // 列表项绘制的X坐标常量 (X coordinate constant for list item drawing)
    static constexpr auto x = 90.f;
    // 列表项绘制的Y坐标偏移量 (Y coordinate offset for list item drawing)
    auto y = this->mod_yoff;
    
    // 获取当前选中游戏的MOD信息 (Get MOD information of currently selected game)
    const auto& current_game = this->entries[this->index];
    
    

    // 获取映射后的游戏名称，如果没有映射则使用原始名称 (Get mapped game name, use original name if no mapping)
    // 绘制标题(Draw title)
    SOFTWARE_TITLE = this->entries[this->index].FILE_NAME2;


    // 遍历并绘制当前游戏的MOD列表项 (Iterate and draw MOD list items of current game)
    for (size_t i = this->mod_start; i < mod_info.size(); i++) {
        // 检查是否为当前光标选中的项 (Check if this is the currently cursor-selected item)
        if (i == this->mod_index) {
            // 当前选中项：绘制彩色边框和黑色背景 (Current selected item: draw colored border and black background)
            auto col = pulse.col;  // 获取脉冲颜色 (Get pulse color)
            col.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
            col.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
            col.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
            col.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
            update_pulse_colour(); // 更新脉冲颜色(产生闪烁效果) (Update pulse color for blinking effect)
            // 绘制选中项的彩色边框 (Draw colored border for selected item)
            gfx::drawRect(this->vg, x - 5.f, y - 5.f, box_width + 10.f, box_height + 10.f, col);
            // 绘制选中项的黑色背景 (Draw black background for selected item)
            gfx::drawRect(this->vg, x, y, box_width, box_height, gfx::Colour::BLACK);
        }

        std::string MOD_NAME = mod_info[i].MOD_NAME;
        std::string MOD_TYPE = mod_info[i].MOD_TYPE;
        bool MOD_STATE = mod_info[i].MOD_STATE;

        
        NVGpaint icon_paint; // 声明图标绘制模式变量 (Declare icon paint pattern variable)
        
        switch (MOD_TYPE[1]){
            case 'F':
                MOD_TYPE = FPS_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->FPS_MOD_image, 1.f);
                break;
            case 'G':
                MOD_TYPE = HD_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->HD_MOD_image, 1.f);
                break;
            case 'C':
                MOD_TYPE = CHEAT_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->Cheat_code_MOD_image, 1.f);
                break;
            case 'P':
                MOD_TYPE = PLAY_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->More_PLAY_MOD_image, 1.f);
                break;
            case 'B':
                MOD_TYPE = COSMETIC_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->Cosmetic_MOD_image, 1.f);
                break;
            default:
                MOD_TYPE = NONE_TYPE_TEXT;
                icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->NONE_MOD_image, 1.f);
                break;
        }
        
        // 绘制列表项顶部和底部的分隔线 (Draw top and bottom separator lines for list item)
        gfx::drawRect(this->vg, x, y, box_width, 1.f, gfx::Colour::DARK_GREY);
        gfx::drawRect(this->vg, x, y + box_height, box_width, 1.f, gfx::Colour::DARK_GREY);

        

        // 创建并绘制应用图标 (Create and draw application icon)
        // const auto icon_paint = nvgImagePattern(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, 0.f, this->default_icon_image, 1.f);
        gfx::drawRect(this->vg, x + icon_spacing, y + icon_spacing, 90.f, 90.f, icon_paint);

        // 保存当前绘图状态并设置文本裁剪区域 (Save current drawing state and set text clipping area)
        nvgSave(this->vg);
        nvgScissor(this->vg, x + title_spacing_left, y, 585.f, box_height); // clip

        // 绘制MOD名称，防止文本溢出 (Draw MOD name, preventing text overflow)
        // 优先显示映射后的名称，如果没有映射则显示原始名称 (Prioritize mapped name, show original if no mapping)
        gfx::drawText(this->vg, x + title_spacing_left, y + title_spacing_top, 24.f, mod_info[i].MOD_NAME2.c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE);
        // 恢复之前保存的绘图状态 (Restore previously saved drawing state)
        nvgRestore(this->vg);

        // 绘制版本信息 (Draw version information)
        gfx::drawTextArgs(this->vg, x + text_spacing_left + 0.f, y + text_spacing_top + 9.f, 22.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::SILVER, MOD_TYPE_TEXT.c_str(), MOD_TYPE.c_str());

        // 绘制模组状态 (Draw mod status)
        if (MOD_STATE){
            gfx::drawTextArgs(this->vg, 755.f, y + box_height / 2.f, 26.f, 
                NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, gfx::Colour::CYAN, MOD_STATE_INSTALLED.c_str());
        }else{
            gfx::drawTextArgs(this->vg, 755.f, y + box_height / 2.f, 26.f, 
                NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, gfx::Colour::SILVER, MOD_STATE_UNINSTALLED.c_str());
        }
        

        
        // 更新Y坐标为下一项位置 (Update Y coordinate to next item position)
        y += box_height;

        // 超出可视区域时停止绘制 (Stop drawing when out of visible area)
        if ((y + box_height) > 646.f) {
            break;
        }
    }



    // 绘制当前选中游戏的图标到右侧信息框左上角 (Draw current selected game icon to top-left of right info box)
    // 计算图标绘制位置：信息框左上角 + 偏移量 (Calculate icon position: info box top-left + offset)
    const float icon_x = sidebox_x + 35.f;  // 距离信息框左边35像素 (35 pixels from left edge of info box)
    const float icon_y = sidebox_y + 35.f;  // 距离信息框顶部35像素 (35 pixels from top edge of info box)
    const float icon_size = 130.f;          // 图标尺寸 (Icon size)
        
    // 绘制半透明背景遮罩层，增加层次感 (Draw semi-transparent background mask for depth)
    const NVGcolor mask_color = nvgRGBA(0, 0, 0, 30);  // 半透明黑色遮罩 (Semi-transparent black mask)
    // 使用圆角矩形绘制阴影，圆角半径7像素 (Draw shadow with rounded corners, 7px radius)
    nvgBeginPath(this->vg);
    nvgRoundedRect(this->vg, icon_x - 2.f, icon_y - 2.f, icon_size + 4.f, icon_size + 4.f, 7.f);
    nvgFillColor(this->vg, mask_color);
    nvgFill(this->vg);
        
    // 创建当前选中游戏的图标绘制模式，使用轻微透明度 (Create image pattern with slight transparency)
    const auto selected_icon_paint = nvgImagePattern(this->vg, icon_x, icon_y, icon_size, icon_size, 0.f, current_game.image, 0.9f);
        
    // 绘制当前选中游戏的图标 (Draw current selected game icon)
    // 使用圆角矩形绘制图标 (Draw icon with rounded corners)
    nvgBeginPath(this->vg);
    nvgRoundedRect(this->vg, icon_x, icon_y, icon_size, icon_size, 5.f); // 5像素圆角 (5 pixel corner radius)
    nvgFillPaint(this->vg, selected_icon_paint);
    nvgFill(this->vg);

    if (current_game.is_favorite){
        // 计算收藏图标位置：大游戏图标右上角，上右边距8像素 (Calculate favorite icon position: top-right corner of game icon, 8px margin from top and right)
        const float heart_size = 25.f;  // 收藏图标尺寸 (Favorite icon size)
        const float heart_margin = 8.f; // 边距 (Margin)
        const float heart_x = icon_x + icon_size - heart_size - heart_margin;  // 右边距8像素 (8px from right edge)
        const float heart_y = icon_y + heart_margin;  // 上边距8像素 (8px from top edge)
        
        // 创建收藏图标绘制模式 (Create favorite icon paint pattern)
        const auto heart_paint = nvgImagePattern(this->vg, heart_x, heart_y, heart_size, heart_size, 0.f, this->like_image, 1.f);
        
        // 绘制收藏图标 (Draw favorite icon)
        gfx::drawRect(this->vg, heart_x, heart_y, heart_size, heart_size, heart_paint);
    }

    // 绘制版本检测状态 (Draw version staus)
    gfx::drawTextArgs(this->vg, icon_x + icon_size + 30.f, icon_y + icon_size / 4 -10.f, 
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE, GAME_VERSION.c_str(), current_game.display_version.c_str());
    gfx::drawTextArgs(this->vg, icon_x + icon_size + 30.f, icon_y + icon_size / 2 , 
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE, MOD_VERSION.c_str(), current_game.MOD_VERSION.c_str());

    if (current_game.MOD_VERSION == NONE_TYPE_TEXT || current_game.display_version == NONE_GAME_TEXT){
        gfx::drawTextArgs(this->vg, icon_x + icon_size + 30.f, icon_y + icon_size * 3 / 4 + 10.f, 
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE, VERSION_NONE.c_str());
    }else if (CompareModGameVersion(current_game.MOD_VERSION, current_game.display_version)){
        gfx::drawTextArgs(this->vg, icon_x + icon_size + 30.f, icon_y + icon_size * 3 / 4 + 10.f, 
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::TEAL, VERSION_OK.c_str());
    }else {
        gfx::drawTextArgs(this->vg, icon_x + icon_size + 30.f, icon_y + icon_size * 3 / 4 + 10.f, 
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::RED, VERSION_ERROR.c_str());
    }

    gfx::drawTextArgs(this->vg, icon_x, icon_y + icon_size + 40.f,
        21.f, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::CYAN, MOD_CONTEXT.c_str());



    // 获取当前选中模组的信息 (Get current selected mod information)
    std::string MOD_TYPE_TEXT;

    MOD_TYPE_TEXT = GetModDescription(mod_info[this->mod_index].MOD_NAME, mod_info[this->mod_index].MOD_TYPE);

    if (MOD_TYPE_TEXT == ""){
        switch ((mod_info[this->mod_index].MOD_TYPE)[1]) {
            case 'F':
                MOD_TYPE_TEXT = FPS_TEXT;
                break;
            case 'G':
                MOD_TYPE_TEXT = HD_TEXT;
                break;
            case 'C':
                MOD_TYPE_TEXT = CHEAT_TEXT;
                break;
            case 'P':
                MOD_TYPE_TEXT = PLAY_TEXT;
                break;
            case 'B':
                MOD_TYPE_TEXT = COSMETIC_TEXT;
                break;
            default:
                MOD_TYPE_TEXT = NONE_TEXT;
                break;
        }
    }
        
        
    // 在右侧信息框绘制当前选中模组的详细信息（左对齐，顶部对齐）
    gfx::drawTextBoxCentered(this->vg, 
            icon_x ,                                 // X坐标
            icon_y + icon_size + 40.f + 14.f,             // Y坐标  
            sidebox_w - 70.f,                     // 宽度（减去左右边距）
            sidebox_h - icon_size - 35.f - 40.f - 21.f -22.f,         // 高度
            21.f,                                 // 字体大小
            1.5f,                                 // 行间距倍数
            MOD_TYPE_TEXT.c_str(),             // 当前选中模组的文本
            nullptr, 
            NVG_ALIGN_LEFT | NVG_ALIGN_TOP,       // 左对齐，顶部对齐
            gfx::Colour::WHITE
    );


    // 绘制左下角mod数量 (Draw mod count in bottom-left corner)
    gfx::drawTextArgs(this->vg, 55.f, 670.f, 24.f, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE, "%zu / %zu", this->mod_index + 1, mod_info.size());

    // 恢复NanoVG绘图状态 / Restore NanoVG drawing state
    nvgRestore(this->vg);

    gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_BACK.c_str()}, 
            gfx::pair{gfx::Button::A, BUTTON_SELECT.c_str()},
            gfx::pair{gfx::Button::X, OPTION_BUTTON.c_str()},
            gfx::pair{gfx::Button::Y, CLEAR_BUTTON.c_str()}
        );

    

}

// MTP有BUG，很难受，懒得细搞了，瞎逼弄弄拉倒
// MTP传输大点文件容易卡死
void App::DrawMTP() {
    
    SOFTWARE_TITLE = MTP_TITLE_TEXT;
    bool is_active = this->mtp_manager->IsRunning();
    std::string mtp_status_text = is_active ? MTP_STOP_BUTTON : MTP_START_BUTTON;
    gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_BACK.c_str()}, 
            gfx::pair{gfx::Button::X, mtp_status_text.c_str()}
    );

    // MTP管理器已在进入模式时初始化 (MTP manager already initialized when entering mode)
    // 获取MTP状态和传输信息 (Get MTP status and transfer info)
    std::string transfer_status_text = this->mtp_manager->GetTransferInfo();

    if (transfer_status_text == MTP_NOT_RUNNING_TEXT){

        gfx::drawTextBoxCentered(this->vg, 0.f, 0.f, 1280.f, 720.f, 32.f, 1.5f, MTP_NOT_RUNNING_REAL_TEXT.c_str(), nullptr, gfx::Colour::WHITE);
        return;

    } else if (transfer_status_text == MTP_NOUSB_RUNNING_TEXT){

        gfx::drawTextBoxCentered(this->vg, 0.f, 0.f, 1280.f, 720.f, 32.f, 1.5f, MTP_NOUSB_RUNNING_TEXT.c_str(), nullptr, gfx::Colour::TEAL);
        return;

    }
    
    // 提取[XXXX]标签内容用于判断状态变化 (Extract [XXXX] tag content to determine status changes)
    std::string current_tag = "";
    size_t start_pos = transfer_status_text.find('[');
    size_t end_pos = transfer_status_text.find(']');
    if (start_pos != std::string::npos && end_pos != std::string::npos && end_pos > start_pos) {
        current_tag = transfer_status_text.substr(start_pos, end_pos - start_pos + 1);
    }
    
    // 判断是否需要添加新行：除了[正在写入]和[正在读取]都另起一行 (Determine if new line needed: all except [正在写入] and [正在读取] start new line)
    static std::string last_tag = "";
    static std::string last_full_text = "";
    bool need_new_line = false;
    
    if (!current_tag.empty()) {
        // 除了[正在写入]和[正在读取]，其他标签都另起一行 (All tags except [正在写入] and [正在读取] start new line)
        if (current_tag != MTP_WRITEING_TAG && current_tag != MTP_READING_TAG) {
            // 所有其他标签（包括[写入完成]、[读取完成]等）都另起一行
            if (transfer_status_text != last_full_text) {
                need_new_line = true;
            }
        } else {
            // 对于[正在写入]和[正在读取]，只有第一次出现时才另起一行 (For [正在写入] and [正在读取], only start new line on first appearance)
            if (last_tag != current_tag) {
                need_new_line = true;
            }
        }
        last_tag = current_tag;
    }
    last_full_text = transfer_status_text;
    
    // 更新日志历史：只有包含[]标签的信息才写入日志 (Update log history: only write messages with [] tags to log)
    if (!transfer_status_text.empty() && !current_tag.empty()) {
        if (need_new_line || this->mtp_log_history.empty()) {
            // 添加新行 (Add new line)
            this->mtp_log_history.push_back(transfer_status_text);
            
            // 限制日志条数，超过15条删除最早的 (Limit log entries, remove oldest if over 15)
            if (this->mtp_log_history.size() > 16) {
                this->mtp_log_history.erase(this->mtp_log_history.begin());
            }
        } else if (!this->mtp_log_history.empty()) {
            // 更新最后一行（进度更新等） (Update last line for progress updates)
            this->mtp_log_history.back() = transfer_status_text;
        }
    }
    
    // 显示日志历史 (Display log history)
    if (!this->mtp_log_history.empty()) {
        float y_offset = 126.f;  // 距离标题分界线(86.0f)40像素 (40 pixels from title divider line at 86.0f)
        for (const auto& log_entry : this->mtp_log_history) {
            gfx::drawTextBoxWithColorMarkup(this->vg, 40.f, y_offset, 1200.f, 24.f, 1.5f, 
                            log_entry.c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE);
            y_offset += 30.f;
            
            // 防止超出屏幕，距离底部分界线(646.0f)40像素 (Prevent overflow, 40 pixels from bottom divider line at 646.0f)
            if (y_offset > 606.f) break;
        }
    }

    // 绘制MTP信息已在上面完成 (MTP info drawing completed above)
}    

void App::UpdateMTP() {
 
    // 处理B键返回逻辑 (Handle B key return logic)
    if (this->controller.B) {

        // 如果MTP正在传输文件，先停止MTP (If MTP is running, stop MTP first)
        if (this->mtp_manager->IsTransferActive()) {
            // 显示确认对话框 (Show confirmation dialog)
            this->audio_manager.PlayCancelSound();
            newShowDialogConfirm(MTP_CONFIRM_STOP_TEXT, [this](bool confirmed) {
                if (confirmed) {
                    this->mtp_manager->StopMtp();
                    // 返回到LIST界面 (Return to LIST interface)
                    this->menu_mode = MenuMode::LIST;
                    this->mtp_log_history.clear();
                } 
            });
            return;
        } else if (this->mtp_manager->IsRunning()) {
            this->audio_manager.PlayConfirmSound();
            this->mtp_manager->StopMtp();
        }
        this->audio_manager.PlayConfirmSound();
        // 返回到LIST界面 (Return to LIST interface)
        this->menu_mode = MenuMode::LIST;
        this->mtp_log_history.clear();
        

    } else if (this->controller.X) {

        // 使用新的切换方法 (Use new toggle method)
        bool state = this->mtp_manager->ToggleMtp();
        // 如果返回false，说明有传输任务正在进行，需要用户确认 (If returns false, transfer task is in progress, need user confirmation)
        if (!state) {
            this->audio_manager.PlayCancelSound();
            // 显示确认对话框 (Show confirmation dialog)
            newShowDialogConfirm(MTP_CONFIRM_STOP_TEXT, [this](bool confirmed) {
                if (confirmed) this->mtp_manager->StopMtp();
            });
        } else {
            this->audio_manager.PlayConfirmSound();
            this->mtp_log_history.clear();
        }

    }

   

}




// 对比mod版本和游戏版本是否一致的辅助函数 (Helper function to compare mod version and game version consistency)
bool App::CompareModGameVersion(const std::string& mod_version, const std::string& game_version) {
    // 情况1：直接字符串匹配 (Case 1: Direct string match)
    if (mod_version == game_version) {
        return true;
    }
    
    // 情况2：简化版本号对比 - 仅保留数字进行对比 (Case 2: Simplified version comparison - keep only digits for comparison)
    auto simplify_version = [](const std::string& version) -> std::string {
        std::string simplified;
        
        // 仅保留数字字符 (Keep only digit characters)
        for (char c : version) {
            if (std::isdigit(c)) {
                simplified += c;
            }
        }
        
        // 移除前导0 (Remove leading zeros)
        size_t first_non_zero = simplified.find_first_not_of('0');
        if (first_non_zero != std::string::npos) {
            simplified = simplified.substr(first_non_zero);
        } else {
            simplified = "0"; // 全是0的情况 (All zeros case)
        }
        
        // 如果结果为空，返回"0" (If result is empty, return "0")
        if (simplified.empty()) {
            simplified = "0";
        }
        
        return simplified;
    };
    
    std::string simple_mod = simplify_version(mod_version);
    std::string simple_game = simplify_version(game_version);
    
    // 简化后对比 (Compare after simplification)
    return simple_mod == simple_game;
}

// 拼音排序辅助函数：获取游戏名称第一个字符的拼音用于排序 (Pinyin sorting helper function: get pinyin of first character for sorting)
std::string App::GetFirstCharPinyin(const std::string& chinese_text) {
    if (chinese_text.empty()) {
        return ""; // 空字符串直接返回 (Return empty for empty string)
    }
    
    // 特殊处理"传"字 (Special handling for "传" character)
    // 检查是否以"传"字开头 - UTF-8编码的"传"字为 E4 BC A0
    if (chinese_text.length() >= 3 && 
        (unsigned char)chinese_text[0] == 0xE4 && 
        (unsigned char)chinese_text[1] == 0xBC && 
        (unsigned char)chinese_text[2] == 0xA0) {
        return "CHUAN"; // 直接返回"传"的正确拼音 (Directly return correct pinyin for "传")
    }
   
    
    wchar_t wch = 0;
    int bytes = 1;
    
    // UTF-8解码第一个字符 (UTF-8 decode first character)
    unsigned char c = chinese_text[0];
    if (c < 0x80) {
        wch = c;
        bytes = 1;
    } else if ((c & 0xE0) == 0xC0 && chinese_text.length() >= 2) {
        wch = ((c & 0x1F) << 6) | (chinese_text[1] & 0x3F);
        bytes = 2;
    } else if ((c & 0xF0) == 0xE0 && chinese_text.length() >= 3) {
        wch = ((c & 0x0F) << 12) | ((chinese_text[1] & 0x3F) << 6) | (chinese_text[2] & 0x3F);
        bytes = 3;
    } else if ((c & 0xF8) == 0xF0 && chinese_text.length() >= 4) {
        wch = ((c & 0x07) << 18) | ((chinese_text[1] & 0x3F) << 12) | ((chinese_text[2] & 0x3F) << 6) | (chinese_text[3] & 0x3F);
        bytes = 4;
    }
    
    // 检查是否为中文字符并获取拼音 (Check if Chinese character and get pinyin)
    if (WzhePinYin::Pinyin::IsChinese(wch)) {
        auto pinyins = WzhePinYin::Pinyin::GetPinyins(wch);
        if (!pinyins.empty()) {
            return pinyins[0]; // 返回第一个拼音 (Return first pinyin)
        }
    }
    
    // 非中文字符直接返回原字符 (Return original character for non-Chinese)
    return chinese_text.substr(0, bytes);
}

// 游戏目录排序辅助函数 (Game directory sorting helper function)
bool App::gamedirsort(const std::string& a, const std::string& b) {
    // 从缓存中获取映射名称，如果没有则使用目录名 (Get mapped name from cache, use directory name if not found)
    std::string mapped_name_a = game_name_cache.count(a) ? game_name_cache[a].display_name : a;
    std::string mapped_name_b = game_name_cache.count(b) ? game_name_cache[b].display_name : b;
    
    // 获取拼音首字符并进行比较 (Get pinyin first character and compare)
    std::string pinyin_a = GetFirstCharPinyin(mapped_name_a);
    std::string pinyin_b = GetFirstCharPinyin(mapped_name_b);
    
    return pinyin_a < pinyin_b;
}

// 辅助函数：通用的应用条目比较函数
// Helper function: Generic app entry comparison function
bool App::CompareAppEntries(const AppEntry& a, const AppEntry& b, bool reverse_order) {
    // 首先按喜欢状态分组：喜欢的在最前
    // First group by favorite status: favorites first
    if (a.is_favorite != b.is_favorite) {
        return a.is_favorite > b.is_favorite; // 喜欢的在前 (favorites first)
    }
    
    // 然后按安装状态分组：已安装的在前，未安装的在后
    // Then group by installation status: installed first, uninstalled last
    bool a_installed = (a.display_version != NONE_GAME_TEXT);
    bool b_installed = (b.display_version != NONE_GAME_TEXT);
    
    if (a_installed != b_installed) {
        return a_installed > b_installed; // 已安装的在前 (installed first)
    }
    
    // 在同一组内按拼音顺序排序
    // Within the same group, sort by pinyin order
    std::string pinyin_a = this->GetFirstCharPinyin(a.FILE_NAME2);
    std::string pinyin_b = this->GetFirstCharPinyin(b.FILE_NAME2);
    
    if (reverse_order) {
        return pinyin_a < pinyin_b; // A-Z排序 (A-Z order)
    } else {
        return pinyin_a > pinyin_b; // Z-A排序 (Z-A order)
    }
}

void App::Sort()
{
    // std::scoped_lock lock{entries_mutex}; // 保护entries向量的排序操作 (Protect entries vector sorting operation)
    switch (static_cast<SortType>(this->sort_type)) {
        case SortType::Alphabetical:
            // 按应用名称拼音Z-A排序，但先按喜欢-安装状态分组
            // Sort by app name pinyin Z-A, but group by favorite-installation status first
            std::ranges::sort(this->entries, [this](const AppEntry& a, const AppEntry& b) {
                return this->CompareAppEntries(a, b, false); // false表示Z-A排序 (false means Z-A order)
            });
            break;
        case SortType::Alphabetical_Reverse:
            // 按应用名称拼音A-Z排序，但先按喜欢-安装状态分组
            // Sort by app name pinyin A-Z, but group by favorite-installation status first
            std::ranges::sort(this->entries, [this](const AppEntry& a, const AppEntry& b) {
                return this->CompareAppEntries(a, b, true); // true表示A-Z排序 (true means A-Z order)
            });
            break;
        default:
            // 默认按应用名称拼音Z-A排序，但先按喜欢-安装状态分组
            // Default sort by app name pinyin Z-A but group by favorite-installation status first
            std::ranges::sort(this->entries, [this](const AppEntry& a, const AppEntry& b) {
                return this->CompareAppEntries(a, b, false); // false表示Z-A排序 (false means Z-A order)
            });
            break;
    }
}

// ADDGAMELIST界面专用的排序函数 (Dedicated sorting function for ADDGAMELIST interface)
void App::Sort2()
{
    std::scoped_lock lock{entries_AddGame_mutex}; // 保护entries_AddGame向量的排序操作 (Protect entries_AddGame vector sorting operation)
    switch (static_cast<SortType_AddGame>(this->sort_type_AddGame)) {
        case SortType_AddGame::Alphabetical_Reverse:
            // 按游戏名称拼音Z-A排序
            // Sort by game name pinyin Z-A
            std::ranges::sort(this->entries_AddGame, [this](const AppEntry_AddGame& a, const AppEntry_AddGame& b) {
                // 获取游戏名称的拼音首字母
                // Get pinyin of first character of game name
                std::string pinyin_a = this->GetFirstCharPinyin(a.name);
                std::string pinyin_b = this->GetFirstCharPinyin(b.name);
                
                // 按拼音Z-A排序
                // Sort by pinyin Z-A
                return pinyin_a > pinyin_b;
            });
            break;
        case SortType_AddGame::Alphabetical:
            // 按游戏名称拼音A-Z排序
            // Sort by game name pinyin A-Z
            std::ranges::sort(this->entries_AddGame, [this](const AppEntry_AddGame& a, const AppEntry_AddGame& b) {
                // 获取游戏名称的拼音首字母
                // Get pinyin of first character of game name
                std::string pinyin_a = this->GetFirstCharPinyin(a.name);
                std::string pinyin_b = this->GetFirstCharPinyin(b.name);
                
                // 按拼音A-Z排序
                // Sort by pinyin A-Z
                return pinyin_a < pinyin_b;
            });
            break;
        default:
            // 默认按游戏名称拼音A-Z排序
            // Default sort by game name pinyin A-Z
            std::ranges::sort(this->entries_AddGame, [this](const AppEntry_AddGame& a, const AppEntry_AddGame& b) {
                // 获取游戏名称的拼音首字母
                // Get pinyin of first character of game name
                std::string pinyin_a = this->GetFirstCharPinyin(a.name);
                std::string pinyin_b = this->GetFirstCharPinyin(b.name);
                
                // 按拼音A-Z排序
                // Sort by pinyin A-Z
                return pinyin_a < pinyin_b;
            });
            break;
    }
}

const char* App::GetSortStr() {
    switch (static_cast<SortType>(this->sort_type)) {
        case SortType::Alphabetical_Reverse:
            // 返回按字母顺序逆排序的提示文本
            return SORT_ALPHA_ZA.c_str();
        case SortType::Alphabetical:
            // 返回按字母顺序排序的提示文本
            return SORT_ALPHA_AZ.c_str();
        default:
            // 默认返回按容量从大到小排序的提示文本
            return SORT_ALPHA_AZ.c_str();
    }
}

const char* App::GetSortStr2() {
    switch (static_cast<SortType_AddGame>(this->sort_type_AddGame)) {
        case SortType_AddGame::Alphabetical_Reverse:
            // 返回按字母顺序逆排序的提示文本
            return SORT_ALPHA_AZ.c_str();
        case SortType_AddGame::Alphabetical:
            // 返回按字母顺序排序的提示文本
            return SORT_ALPHA_ZA.c_str();
        default:
            // 默认返回按容量从大到小排序的提示文本
            return SORT_ALPHA_ZA.c_str();
    }
}

void App::UpdateLoad() {
    if (this->controller.B) {
        this->async_thread.request_stop();
        this->async_thread.get();
        this->quit = true;
        return;
    }

    {
        std::scoped_lock lock{this->mutex};
        // 只要有应用加载完成就立即显示列表，实现真正的立即显示
        // Show list immediately when any app is loaded, achieving true immediate display
        if (scanned_count.load() > 0 || finished_scanning) {
            if (finished_scanning) {
                this->async_thread.get();
            }
            
            
            this->menu_mode = MenuMode::LIST;
        }
    }
}

void App::UpdateList() {
    
    
    std::scoped_lock lock{entries_mutex}; // 保护entries向量的访问和修改 (Protect entries vector access and modification)
    // 检查应用列表是否为空，避免数组越界访问 (Check if application list is empty to avoid array out-of-bounds access)
    if (this->entries.empty()) {
        // 如果应用列表为空，只处理退出操作 (If application list is empty, only handle exit operation)
        if (this->controller.B) {

            this->audio_manager.PlayKeySound(0.9);
            this->quit = true;

        }else if (this->controller.X) {

            this->audio_manager.PlayConfirmSound(1.0);
            std::vector<std::string> option_text = {
            LIST_DIALOG_MTP,
            LIST_DIALOG_ADDGAME,
            LIST_DIALOG_ABOUTAUTHOR,
            };

            // 创建回调函数来处理选择结果 (Create callback function to handle selection result)
            auto callback = [this](const std::vector<std::string>& selected_items, bool cancelled) {
                if (!cancelled) {
                    this->newListMenuCenter(selected_items[0]);
                }
            };

            newShowDialogListSelect(OPTION_MEMU_TITLE, option_text,false, callback);

        }else if (this->controller.START) {

            // 查看教程
            this->audio_manager.PlayKeySound(0.9);
            this->menu_mode = MenuMode::INSTRUCTION;

        }

        return; // 提前返回，避免后续操作 (Early return to avoid subsequent operations)
    }
    
    // 扫描过程中禁用排序和删除功能
    if (this->controller.B) {
        this->audio_manager.PlayKeySound(0.9);
        this->quit = true;
    } else if (this->controller.A) { // 允许在扫描过程中选择列表项
        // 重置MOD列表的索引变量 (Reset MOD list index variables)
        this->mod_index = 0;
        this->mod_start = 0;
        this->mod_yoff = 130.f;
        this->mod_ypos = 130.f;

        if (!mod_icons_loaded) {
            // 加载所有MOD图标 (Load all MOD icons)
            this->Cheat_code_MOD_image = nvgCreateImage(this->vg, "romfs:/Cheat_code_MOD.jpg", NVG_IMAGE_NEAREST);
            this->Cosmetic_MOD_image = nvgCreateImage(this->vg, "romfs:/Cosmetic_MOD.jpg", NVG_IMAGE_NEAREST);
            this->FPS_MOD_image = nvgCreateImage(this->vg, "romfs:/FPS_MOD.jpg", NVG_IMAGE_NEAREST);
            this->HD_MOD_image = nvgCreateImage(this->vg, "romfs:/HD_MOD.jpg", NVG_IMAGE_NEAREST);
            this->More_PLAY_MOD_image = nvgCreateImage(this->vg, "romfs:/More_PLAY_MOD.jpg", NVG_IMAGE_NEAREST);
            this->NONE_MOD_image = nvgCreateImage(this->vg, "romfs:/NONE_MOD.jpg", NVG_IMAGE_NEAREST);

            mod_icons_loaded = true; // 标记为已加载 (Mark as loaded)
        }
    
        
        const auto& current_game = this->entries[this->index];
        LoadModNameMapping(current_game.FILE_PATH);
        FastScanModInfo();
        Sort_Mod();
        this->audio_manager.PlayKeySound(0.9);
        this->menu_mode = MenuMode::MODLIST;
    } else if (this->controller.X) { // X键显示设置菜单

        this->audio_manager.PlayConfirmSound(); // 播放确认音效 (Play confirm sound effect)
        

        // 检查是否为收藏游戏
        std::string LIST_DIALOG_FAVORITE = IS_FAVORITE;

        if (entries[this->index].is_favorite) LIST_DIALOG_FAVORITE = IS_NOT_FAVORITE;

        std::vector<std::string> option_text = {
            LIST_DIALOG_MTP,
            LIST_DIALOG_CUSTOM_NAME,
            LIST_DIALOG_MODVERSION,
            LIST_DIALOG_FAVORITE,
            LIST_DIALOG_ADDGAME,
            LIST_DIALOG_REMOVE_GAME,
            LIST_DIALOG_ViewDetails,
            LIST_DIALOG_ABOUTAUTHOR,
        };

        // 创建回调函数来处理选择结果 (Create callback function to handle selection result)
        auto callback = [this](const std::vector<std::string>& selected_items, bool cancelled) {
            if (!cancelled) {
                this->newListMenuCenter(selected_items[0]);
            }
        };

        newShowDialogListSelect(OPTION_MEMU_TITLE, option_text,false, callback);

        
        
    } else if (this->controller.DOWN) { // 九宫格向下移动 (Grid move down)
        constexpr auto items_per_page = 9; // 每页9个项目 (9 items per page)
        constexpr auto grid_cols = 3;      // 每行3个项目 (3 items per row)
        
        if (this->index < (this->entries.size() - 1)) {
            // 计算当前在九宫格中的位置 (Calculate current position in grid)
            const size_t current_page = this->index / items_per_page;
            const size_t grid_index = this->index % items_per_page;
            const size_t current_row = grid_index / grid_cols;
            const size_t current_col = grid_index % grid_cols;
            
            // 如果不在最后一行，向下移动一行 (If not in last row, move down one row)
            if (current_row < 2) {
                size_t new_index = this->index + grid_cols;
                if (new_index < this->entries.size()) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = new_index;
                } else {
                    // 如果下一行没有足够的项目，移动到最后一个项目 (If next row doesn't have enough items, move to last item)
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = this->entries.size() - 1;
                }
            } else {
                // 在最后一行，尝试翻到下一页的第一行 (In last row, try to flip to first row of next page)
                size_t next_page_index = (current_page + 1) * items_per_page + current_col;
                if (next_page_index < this->entries.size()) {
                    // 下一页有对应位置的项目，跳转到下一页 (Next page has item at corresponding position, jump to next page)
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = next_page_index;
                } else if ((current_page + 1) * items_per_page < this->entries.size()) {
                    // 下一页存在但没有对应位置的项目，移动到最后一个项目 (Next page exists but no item at corresponding position, move to last item)
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = this->entries.size() - 1;
                } else {
                    // 已经是最后一页且没有更多项目，播放限制音效 (Already last page and no more items, play limit sound)
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.UP) { // 九宫格向上移动 (Grid move up)
        constexpr auto items_per_page = 9; // 每页9个项目 (9 items per page)
        constexpr auto grid_cols = 3;      // 每行3个项目 (3 items per row)
        
        if (this->index > 0) {
            // 计算当前在九宫格中的位置 (Calculate current position in grid)
            const size_t current_page = this->index / items_per_page;
            const size_t grid_index = this->index % items_per_page;
            const size_t current_row = grid_index / grid_cols;
            const size_t current_col = grid_index % grid_cols;
            
            // 如果不在第一行，向上移动一行 (If not in first row, move up one row)
            if (current_row > 0) {
                this->audio_manager.PlayKeySound(0.9);
                this->index -= grid_cols;
            } else {
                // 在第一行，尝试翻到上一页的最后一行 (In first row, try to flip to last row of previous page)
                if (current_page > 0) {
                    size_t prev_page_index = (current_page - 1) * items_per_page + 6 + current_col; // 上一页的最后一行 (Last row of previous page)
                    if (prev_page_index < this->entries.size()) {
                        this->audio_manager.PlayKeySound(0.9);
                        this->index = prev_page_index;
                    } else {
                        // 如果上一页最后一行没有对应列，找到上一页的最后一个项目 (If previous page last row doesn't have corresponding column, find last item of previous page)
                        this->audio_manager.PlayKeySound(0.9);
                        this->index = std::min((current_page * items_per_page) - 1, this->entries.size() - 1);
                    }
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.LEFT) { // 九宫格向左移动 (Grid move left)
        if (this->index > 0) {
            constexpr auto grid_cols = 3; // 每行3个项目 (3 items per row)
            const size_t grid_index = this->index % 9;
            const size_t current_col = grid_index % grid_cols;
            
            // 如果不在第一列，向左移动 (If not in first column, move left)
            if (current_col > 0) {
                this->audio_manager.PlayKeySound(0.9);
                this->index--;
            } else {
                // 在第一列，移动到上一行的最后一列 (In first column, move to last column of previous row)
                if (this->index >= grid_cols) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index -= 1; // 移动到前一个项目 (Move to previous item)
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.RIGHT) { // 九宫格向右移动 (Grid move right)
        if (this->index < (this->entries.size() - 1)) {
            constexpr auto grid_cols = 3; // 每行3个项目 (3 items per row)
            const size_t grid_index = this->index % 9;
            const size_t current_col = grid_index % grid_cols;
            
            // 如果不在最后一列，向右移动 (If not in last column, move right)
            if (current_col < (grid_cols - 1)) {
                size_t new_index = this->index + 1;
                if (new_index < this->entries.size()) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = new_index;
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            } else {
                // 在最后一列，移动到下一行的第一列 (In last column, move to first column of next row)
                size_t new_index = this->index + 1;
                if (new_index < this->entries.size()) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = new_index;
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        } 
    } else if (!is_scan_running && this->controller.Y) { // 非扫描状态下才允许排序
        this->audio_manager.PlayKeySound(); // 播放按键音效 (Play key sound)
        this->sort_type++;

        if (this->sort_type == std::to_underlying(SortType::MAX)) {
            this->sort_type = 0;
        }

        this->Sort();
        
        // 强制重置可见区域缓存，确保排序后图标能重新加载 (Force reset visible range cache to ensure icons reload after sorting)
        this->last_loaded_range = {SIZE_MAX, SIZE_MAX};
        
        // 重置选择索引，使选择框回到第一项 (Reset selection index to first item)
        this->index = 0;

    } else if (this->controller.L) { // L键向上翻页 (L key for page up)
        if (this->entries.size() > 0) {
            constexpr auto items_per_page = 9; // 每页9个项目 (9 items per page)
            
            // 计算当前页码和在页面中的位置 (Calculate current page number and position in page)
            const size_t current_page = this->index / items_per_page;
            const size_t position_in_page = this->index % items_per_page;
            
            if (current_page > 0) {
                // 翻到上一页的相同位置 (Flip to same position in previous page)
                const size_t target_index = (current_page - 1) * items_per_page + position_in_page;
                
                // 检查目标位置是否存在 (Check if target position exists)
                if (target_index < this->entries.size()) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = target_index;
                } else {
                    // 如果目标位置不存在，选择上一页的最后一个项目 (If target position doesn't exist, select last item of previous page)
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = std::min((current_page * items_per_page) - 1, this->entries.size() - 1);
                }
            } else {
                // 已经在第一页，移动到第一个项目 (Already on first page, move to first item)
                if (this->index != 0) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = 0;
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        }
    } else if (this->controller.R) { // R键向下翻页 (R key for page down)
        if (this->entries.size() > 0) {
            constexpr auto items_per_page = 9; // 每页9个项目 (9 items per page)
            
            // 计算当前页码、总页数和在页面中的位置 (Calculate current page number, total pages and position in page)
            const size_t current_page = this->index / items_per_page;
            const size_t total_pages = (this->entries.size() + items_per_page - 1) / items_per_page;
            const size_t position_in_page = this->index % items_per_page;
            
            if (current_page < (total_pages - 1)) {
                // 翻到下一页的相同位置 (Flip to same position in next page)
                const size_t target_index = (current_page + 1) * items_per_page + position_in_page;
                
                // 检查目标位置是否存在 (Check if target position exists)
                if (target_index < this->entries.size()) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = target_index;
                } else {
                    // 如果目标位置不存在，选择最后一个项目 (If target position doesn't exist, select last item)
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = this->entries.size() - 1;
                }
            } else {
                // 已经在最后一页，移动到最后一个项目 (Already on last page, move to last item)
                if (this->index != (this->entries.size() - 1)) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index = this->entries.size() - 1;
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        }
    } else if (this->controller.START) { // +键进入教程)
        // 查看教程
        this->audio_manager.PlayKeySound(0.9);
        this->menu_mode = MenuMode::INSTRUCTION;
    } else if (!is_scan_running && this->controller.SELECT) { 
        // 搜索界面
        this->audio_manager.PlayConfirmSound();
        
        // 创建局部字符串数组，将entries的FILE_NAME2传入 (Create local string array with FILE_NAME2 from entries)
        std::vector<std::string> search_context_array;
        
        search_context_array.reserve(this->entries.size()); // 预分配空间 (Pre-allocate space)
        for (const auto& entry : this->entries) {
            search_context_array.push_back(entry.FILE_NAME2);
        }
        
        // 定义搜索结果选择回调函数 (Define search result selection callback function)
        DialogSearchCallback search_callback = [this](int selected_index) {
            // 直接设置主列表索引 (Directly set main list index)
            this->index = static_cast<size_t>(selected_index);
        };
        
        // 传入搜索对话框和回调函数 (Pass to search dialog with callback function)
        newShowDialogSearch(search_context_array, search_callback);
    } 

    
}



void App::UpdateModList() {
    
    
    // 如果当前游戏没有MOD，只处理返回操作 (If current game has no MODs, only handle back operation)
    if (mod_info.empty()) {
        if (this->controller.B) {
            this->audio_manager.PlayKeySound(0.9);
            this->menu_mode = MenuMode::LIST;
        }
        return;
    }

    if (this->controller.B) {

        this->audio_manager.PlayKeySound(0.9);
        this->menu_mode = MenuMode::LIST;

    } else if (this->controller.A) {

        this->audio_manager.PlayConfirmSound();
        MODinstallORuninstall(); // 进入是否卸载的逻辑判断

    } else if (this->controller.Y) { // Y键清理缓存 (Y key to clear cache)
        
        this->audio_manager.PlayConfirmSound();
        this->clean_button = true;
        MODinstallORuninstall(); // 进入是否卸载的逻辑判断

    } else if (this->controller.X) { // X键清选项菜单

        this->audio_manager.PlayConfirmSound(); // 播放确认音效 (Play confirm sound effect)

        std::vector<std::string> option_text = {
            LIST_DIALOG_CUSTOM_NAME,
            LIST_DIALOG_MODTYPE,
            LIST_DIALOG_MOD_DESCRIPTION,
            LIST_DIALOG_APPENDMOD,
            LIST_DIALOG_REMOVE_MOD,
            LIST_DIALOG_ViewDetails,
        };

        // 创建回调函数来处理选择结果 (Create callback function to handle selection result)
        auto callback = [this](const std::vector<std::string>& selected_items, bool cancelled) {
            if (!cancelled) {
                this->newListMenuCenter(selected_items[0]);
            }
        };

        newShowDialogListSelect(OPTION_MEMU_TITLE, option_text,false, callback);

    } else if (this->controller.DOWN) { // 向下移动 (Move down)
        if (this->mod_index < (mod_info.size() - 1)) {
            this->audio_manager.PlayKeySound(0.9);
            this->mod_index++;
            this->mod_ypos += this->BOX_HEIGHT;
            if ((this->mod_ypos + this->BOX_HEIGHT) > 646.f) {
                this->mod_ypos -= this->BOX_HEIGHT;
                this->mod_yoff = this->mod_ypos - ((this->mod_index - this->mod_start - 1) * this->BOX_HEIGHT);
                this->mod_start++;
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.UP) { // 向上移动 (Move up)
        if (this->mod_index != 0 && mod_info.size()) {
            this->audio_manager.PlayKeySound(0.9);
            this->mod_index--;
            this->mod_ypos -= this->BOX_HEIGHT;
            if (this->mod_ypos < 86.f) {
                this->mod_ypos += this->BOX_HEIGHT;
                this->mod_yoff = this->mod_ypos;
                this->mod_start--;
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.L) { // L键向上翻页 (L key for page up)
        if (mod_info.size() > 0) {
            // 直接更新index，向上翻页4个位置 (Directly update index, page up 4 positions)
            if (this->mod_index >= 4) {
                this->audio_manager.PlayKeySound(0.9); // 正常翻页音效 (Normal page flip sound)
                this->mod_index -= 4;
            } else {
                this->mod_index = 0;
                this->audio_manager.PlayLimitSound(1.5);
            }
            
            // 直接更新mod_start位置，确保翻页显示4个项目 (Directly update mod_start position to ensure 4 items per page)
            if (this->mod_start >= 4) {
                this->mod_start -= 4;
            } else {
                this->mod_start = 0;
            }
            
            // 边界检查：确保mod_index和mod_start的一致性 (Boundary check: ensure consistency between mod_index and mod_start)
            if (this->mod_index < this->mod_start) {
                this->mod_index = this->mod_start;
            }
            
            // 边界检查：确保index不会超出当前页面范围 (Boundary check: ensure index doesn't exceed current page range)
            std::size_t max_index_in_page = this->mod_start + 3; // 当前页面最大索引 (Maximum index in current page)
            if (this->mod_index > max_index_in_page && max_index_in_page < mod_info.size()) {
                this->mod_index = max_index_in_page;
            }
            
            // 更新相关显示变量 (Update related display variables)
            this->mod_ypos = 130.f + (this->mod_index - this->mod_start) * this->BOX_HEIGHT;
            this->mod_yoff = 130.f;
        }
    } else if (this->controller.R) { // R键向下翻页 (R key for page down)
        if (mod_info.size() > 0) {
            // 直接更新mod_index，向下翻页4个位置 (Directly update mod_index, page down 4 positions)
            this->mod_index += 4;
            
            // 边界检查：确保不超出列表范围 (Boundary check: ensure not exceeding list range)
            if (this->mod_index >= mod_info.size()) {
                this->mod_index = mod_info.size() - 1;
                this->audio_manager.PlayLimitSound(1.5);
            } else {
                this->audio_manager.PlayKeySound(0.9);
            }
            
            // 直接更新mod_start位置，确保翻页显示4个项目 (Directly update mod_start position to ensure 4 items per page)
            this->mod_start += 4;
            
            // 边界检查：确保mod_start不会超出合理范围 (Boundary check: ensure mod_start doesn't exceed reasonable range)
            if (mod_info.size() > 4) {
                std::size_t max_start = mod_info.size() - 4;
                if (this->mod_start > max_start) {
                    this->mod_start = max_start;
                    // 当到达末尾时，调整mod_index到最后一个可见项 (When reaching end, adjust mod_index to last visible item)
                    this->mod_index = mod_info.size() - 1;
                }
            } else {
                this->mod_start = 0;
            }
            
            // 更新相关显示变量 (Update related display variables)
            this->mod_ypos = 130.f + (this->mod_index - this->mod_start) * this->BOX_HEIGHT;
            this->mod_yoff = 130.f;
        }
    }
}

// 更新添加游戏列表界面 (Update add game list interface)
void App::UpdateAddGameList() {
    
    
    // 检查是否需要启动扫描 (Check if scan needs to be started)
    bool need_start_scan = false;
    {
        std::lock_guard<std::mutex> lock(this->entries_AddGame_mutex);
        need_start_scan = this->entries_AddGame.empty() && !addgame_scan_running.load();
    }
    
    // 每次进入AddGame界面都重新扫描 (Rescan every time entering AddGame interface)
    if (need_start_scan) {
        // 清空现有列表和重置状态 (Clear existing list and reset state)
        {
            std::lock_guard<std::mutex> lock(this->entries_AddGame_mutex);
            this->entries_AddGame.clear();
        }
        
        // 重置扫描计数器 (Reset scan counters)
        addgame_scanned_count = 0;
        addgame_total_count = 0;
        
        // 启动异步扫描 (Start async scan)
        this->async_thread = util::async([this](std::stop_token stop_token) {
            this->FastScanAllGames(stop_token);
        });
    }

    if (!addgame_scan_running.load() && this->entries_AddGame.empty()) {
        if (this->controller.B) {

            clearaddgamelist();
            // 返回主界面 (Return to main interface)
            this->audio_manager.PlayKeySound(0.9);
            this->menu_mode = MenuMode::LIST;
        
        }
        return;
    }
    
    // 处理控制器输入 (Handle controller input)
    if (this->controller.B) {
        // 主动停止扫描线程 (Actively stop scanning thread)
        if (this->async_thread.valid()) {
            this->async_thread.request_stop(); // 请求线程停止 (Request thread to stop)
            this->async_thread.get(); // 等待线程完成 (Wait for thread completion)
        }
        this->sort_type_AddGame = 0;
        // 清理AddGame界面资源和状态 (Clear AddGame interface resources and states)
        clearaddgamelist();
        // 返回主界面 (Return to main interface)
        this->audio_manager.PlayKeySound(0.9);
        this->menu_mode = MenuMode::LIST;
        
        return;
    }
    
    // 获取当前列表大小 (Get current list size)
    size_t list_size = 0;
    {
        std::lock_guard<std::mutex> lock(this->entries_AddGame_mutex);
        list_size = this->entries_AddGame.size();
    }
    
    // 如果列表为空，只处理返回操作 (If list is empty, only handle back operation)
    if (list_size == 0) {
        return;
    }
    
    // 处理九宫格导航 (Handle grid navigation)
    const int GRID_COLS = 3; // 九宫格列数 (Grid columns)
    const int GRID_ROWS = 3; // 九宫格行数 (Grid rows)
    const int ITEMS_PER_PAGE = GRID_COLS * GRID_ROWS; // 每页项目数 (Items per page)
    
    if (this->controller.DOWN) { // 向下移动 (Move down)
        if (list_size > 0) {
            int current_row = (this->index_AddGame % ITEMS_PER_PAGE) / GRID_COLS;
            int current_col = (this->index_AddGame % ITEMS_PER_PAGE) % GRID_COLS;
            
            if (current_row < GRID_ROWS - 1) {
                // 在当前页内向下移动 (Move down within current page)
                int new_index = this->index_AddGame + GRID_COLS;
                if (new_index < static_cast<int>(list_size)) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index_AddGame = new_index;
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            } else {
                // 尝试翻到下一页 (Try to go to next page)
                int next_page_start = ((this->index_AddGame / ITEMS_PER_PAGE) + 1) * ITEMS_PER_PAGE;
                int new_index = next_page_start + current_col;
                if (new_index < static_cast<int>(list_size)) {
                    // 下一页有对应位置的项目，跳转到下一页 (Next page has item at corresponding position, jump to next page)
                    this->audio_manager.PlayKeySound(0.9);
                    this->index_AddGame = new_index;
                } else if (next_page_start < static_cast<int>(list_size)) {
                    // 下一页存在但没有对应位置的项目，移动到最后一个项目 (Next page exists but no item at corresponding position, move to last item)
                    this->audio_manager.PlayKeySound(0.9);
                    this->index_AddGame = static_cast<int>(list_size) - 1;
                } else {
                    // 已经是最后一页且没有更多项目，播放限制音效 (Already last page and no more items, play limit sound)
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        }
    } else if (this->controller.UP) { // 向上移动 (Move up)
        if (list_size > 0 && this->index_AddGame > 0) {
            int current_row = (this->index_AddGame % ITEMS_PER_PAGE) / GRID_COLS;
            int current_col = (this->index_AddGame % ITEMS_PER_PAGE) % GRID_COLS;
            
            if (current_row > 0) {
                // 在当前页内向上移动 (Move up within current page)
                this->audio_manager.PlayKeySound(0.9);
                this->index_AddGame -= GRID_COLS;
            } else {
                // 尝试翻到上一页 (Try to go to previous page)
                if (this->index_AddGame >= ITEMS_PER_PAGE) {
                    int prev_page_start = ((this->index_AddGame / ITEMS_PER_PAGE) - 1) * ITEMS_PER_PAGE;
                    int new_index = prev_page_start + (GRID_ROWS - 1) * GRID_COLS + current_col;
                    this->audio_manager.PlayKeySound(0.9);
                    this->index_AddGame = new_index;
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.LEFT) { // 向左移动 (Move left)
        if (list_size > 0) {
            if (this->index_AddGame > 0) {
                // 可以向左移动，支持换行 (Can move left, supports line wrapping)
                this->audio_manager.PlayKeySound(0.9);
                this->index_AddGame--;
            } else {
                // 已经在第一个位置，播放边界音效 (Already at first position, play boundary sound)
                this->audio_manager.PlayLimitSound(1.5);
            }
        }
    } else if (this->controller.RIGHT) { // 向右移动 (Move right)
        if (list_size > 0) {
            if (this->index_AddGame + 1 < static_cast<int>(list_size)) {
                // 可以向右移动，支持换行 (Can move right, supports line wrapping)
                this->audio_manager.PlayKeySound(0.9);
                this->index_AddGame++;
            } else {
                // 已经在最后一个位置，播放边界音效 (Already at last position, play boundary sound)
                this->audio_manager.PlayLimitSound(1.5);
            }
        }
    } else if (this->controller.L) { // L键向上翻页 (L key for page up)
        if (list_size > 0) {
            // 计算当前页码和在页面中的位置 (Calculate current page number and position in page)
            const size_t current_page = this->index_AddGame / ITEMS_PER_PAGE;
            const size_t position_in_page = this->index_AddGame % ITEMS_PER_PAGE;
            
            if (current_page > 0) {
                // 翻到上一页的相同位置 (Flip to same position in previous page)
                const size_t target_index = (current_page - 1) * ITEMS_PER_PAGE + position_in_page;
                
                // 检查目标位置是否存在 (Check if target position exists)
                if (target_index < list_size) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index_AddGame = target_index;
                } else {
                    // 如果目标位置不存在，选择上一页的最后一个项目 (If target position doesn't exist, select last item of previous page)
                    this->audio_manager.PlayKeySound(0.9);
                    this->index_AddGame = std::min((current_page * ITEMS_PER_PAGE) - 1, list_size - 1);
                }
                // 更新start_AddGame以确保正确显示 (Update start_AddGame to ensure correct display)
                this->start_AddGame = (this->index_AddGame / ITEMS_PER_PAGE) * ITEMS_PER_PAGE;
            } else {
                // 已经在第一页，移动到第一个项目 (Already on first page, move to first item)
                if (this->index_AddGame != 0) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index_AddGame = 0;
                    this->start_AddGame = 0;
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        }
    } else if (this->controller.R) { // R键向下翻页 (R key for page down)
        if (list_size > 0) {
            // 计算当前页码、总页数和在页面中的位置 (Calculate current page number, total pages and position in page)
            const size_t current_page = this->index_AddGame / ITEMS_PER_PAGE;
            const size_t total_pages = (list_size + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
            const size_t position_in_page = this->index_AddGame % ITEMS_PER_PAGE;
            
            if (current_page < (total_pages - 1)) {
                // 翻到下一页的相同位置 (Flip to same position in next page)
                const size_t target_index = (current_page + 1) * ITEMS_PER_PAGE + position_in_page;
                
                // 检查目标位置是否存在 (Check if target position exists)
                if (target_index < list_size) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index_AddGame = target_index;
                } else {
                    // 如果目标位置不存在，选择最后一个项目 (If target position doesn't exist, select last item)
                    this->audio_manager.PlayKeySound(0.9);
                    this->index_AddGame = list_size - 1;
                }
                // 更新start_AddGame以确保正确显示 (Update start_AddGame to ensure correct display)
                this->start_AddGame = (this->index_AddGame / ITEMS_PER_PAGE) * ITEMS_PER_PAGE;
            } else {
                // 已经在最后一页，移动到最后一个项目 (Already on last page, move to last item)
                if (this->index_AddGame != (list_size - 1)) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->index_AddGame = list_size - 1;
                    this->start_AddGame = (this->index_AddGame / ITEMS_PER_PAGE) * ITEMS_PER_PAGE;
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        }
    } else if (this->controller.Y && !addgame_scan_running.load()) { 
        this->audio_manager.PlayKeySound(0.9); // 播放按键音效 (Play key sound)
        this->sort_type_AddGame++;

        if (this->sort_type_AddGame == std::to_underlying(SortType_AddGame::MAX)) {
            this->sort_type_AddGame = 0;
        }

        Sort2();
        
        // 强制重置ADDGAMELIST界面的可见区域缓存，确保排序后图标能重新加载 (Force reset AddGame interface visible range cache to ensure icons reload after sorting)
        this->last_addgame_loaded_range = {SIZE_MAX, SIZE_MAX};
        
        // 重置选择索引，使选择框回到第一项 (Reset selection index to first item)
        this->index_AddGame = 0;
        this->start_AddGame = 0;
    } else if (this->controller.A) { // A键确认选择 (A key for confirm selection)
        this->audio_manager.PlayConfirmSound(1.0);

        // 创建回调函数来处理选择结果 (Create callback function to handle selection result)
        auto callback = [this](const std::vector<std::string>& selected_items, bool cancelled) {
            if (!cancelled) {
                this->Dialog_ADDGAME_Menu(selected_items);
            }
        };

        newShowDialogListSelect(OPTION_APPENDMOD_MEMU_TITLE, appendmodscan(),true, callback);

    } else if (this->controller.SELECT) { 
        // 搜索界面
        this->audio_manager.PlayConfirmSound();
        
        // 创建局部字符串数组，将entries的FILE_NAME2传入 (Create local string array with FILE_NAME2 from entries)
        std::vector<std::string> search_context_array;

        {
            std::lock_guard<std::mutex> lock(this->entries_AddGame_mutex);
            search_context_array.reserve(this->entries_AddGame.size()); // 预分配空间 (Pre-allocate space)
            for (const auto& entry : this->entries_AddGame) {
                search_context_array.push_back(entry.name);
            }
        }
        
        // 定义搜索结果选择回调函数 (Define search result selection callback function)
        DialogSearchCallback search_callback = [this](int selected_index) {
            // 直接设置主列表索引 (Directly set main list index)
            this->index_AddGame = static_cast<size_t>(selected_index);
        };
        
        // 传入搜索对话框和回调函数 (Pass to search dialog with callback function)
        newShowDialogSearch(search_context_array, search_callback);
    }


}


std::string App::TryGetAppEnglishName(u64 application_id){
    // 直接从NS服务获取应用控制数据
    // Get application control data directly from NS service
    auto control_data = std::make_unique<NsApplicationControlData>();
    u64 jpeg_size{}; // JPEG数据大小 (JPEG data size)
    
    Result result = nsGetApplicationControlData(NsApplicationControlSource_Storage, application_id, control_data.get(), sizeof(NsApplicationControlData), &jpeg_size);
    if (R_FAILED(result)) {
        // 获取失败，返回16位十六进制格式的ID字符串
        // Failed to get data, return 16-digit hex format ID string
        char app_id_buffer[17]; // 16位十六进制字符串 + 结束符 (16-digit hex string + null terminator)
        snprintf(app_id_buffer, sizeof(app_id_buffer), "%016lX", application_id);
        return std::string(app_id_buffer);
    }
    
    // 遍历索引0（美式英语）和1（英式英语），找到第一个非空且真正为英文的名字
    // Iterate through index 0 (American English) and 1 (British English), find first non-empty and truly English name
    for (int i = 0; i <= 1; i++) {
        NacpLanguageEntry* english_entry = &control_data->nacp.lang[i];
        
        // 检查英文名称是否存在且不为空
        // Check if English name exists and is not empty
        if (english_entry->name[0] != '\0') {
            // 使用IsEnglishText检查名称是否真的是英文，避免厂家填入其他语言
            // Use IsEnglishText to check if the name is truly English, avoiding manufacturers filling in other languages
            std::string validated_name = IsEnglishText(english_entry->name);
            if (!validated_name.empty()) {
                return validated_name;
            }
        }
    }
    
    // 两个英文索引都为空，返回16位十六进制格式的ID字符串
    // Both English indices are empty, return 16-digit hex format ID string
    char app_id_buffer[17]; // 16位十六进制字符串 + 结束符 (16-digit hex string + null terminator)
    snprintf(app_id_buffer, sizeof(app_id_buffer), "%016lX", application_id);
    return std::string(app_id_buffer);
}


// 检测字符串是否100%为英文字符，符合返回处理后的字符串（标点符号替换为空格），不符合返回空字符串
// Detect if the string is 100% English characters, return processed string (punctuation replaced with spaces) if valid, empty string if not
std::string App::IsEnglishText(const char text[512]) {
    if (text == nullptr || text[0] == '\0') {
        return ""; // 空指针或空字符串返回空字符串 (Return empty string for null pointer or empty string)
    }
    
    std::string result; // 存储处理后的结果字符串 (Store the processed result string)
    
    for (int i = 0; i < 512 && text[i] != '\0'; i++) {
        char c = text[i];
        // 检查是否为ASCII字符范围内的英文字符
        // Check if it's an English character within ASCII range
        if (static_cast<unsigned char>(c) > 127) {
            return ""; // 非ASCII字符，不是英文，返回空字符串
        }
        
        // 标点符号替换为空格，其他字符直接添加
        // Replace punctuation with space, add other characters directly
        if (c == '.' || c == ',' || c == '!' || c == '?' || 
            c == ':' || c == ';' || c == '\'' || c == '"' || c == '-' || 
            c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || 
            c == '}' || c == '\n' || c == '\r' || c == '_' || 
            c == '+' || c == '=' || c == '*' || c == '/' || c == '\\' || 
            c == '|' || c == '@' || c == '#' || c == '$' || c == '%' || 
            c == '^' || c == '&' || c == '<' || c == '>') {
            result += '-'; // 标点符号替换为空格
        } else {
            result += c; // 其他ASCII字符直接添加
        }
    }
    
    return result; // 返回处理后的字符串 (Return processed string)
}





// 快速获取应用基本信息并缓存图标数据
// Fast get application basic info and cache icon data
bool App::TryGetAppBasicInfoWithIconCache(u64 application_id, AppEntry& entry) {
    
    u32 version_32 = getgameversioninfo(application_id);

    // 1. 优先尝试从缓存获取应用名称
    // 1. Try to get application name from cache first
    NxTitleCacheApplicationMetadata* cached_metadata = nxtcGetApplicationMetadataEntryById(application_id);
    if (cached_metadata != nullptr && version_32 == cached_metadata->version_info) {
        entry.name = cached_metadata->name;
        entry.id = cached_metadata->title_id;
        entry.display_version = cached_metadata->version; 
        // 暂时设置默认值，稍后异步加载
        // Set default values temporarily, load asynchronously later
        entry.image = this->default_icon_image;
        entry.own_image = false;
        
        // 缓存图标数据到AppEntry中
        // Cache icon data in AppEntry
        if (cached_metadata->icon_data && cached_metadata->icon_size > 0) {
            entry.cached_icon_data.resize(cached_metadata->icon_size);
            memcpy(entry.cached_icon_data.data(), cached_metadata->icon_data, cached_metadata->icon_size);
            entry.has_cached_icon = true;
        } else {
            entry.has_cached_icon = false;
        }
        
        nxtcFreeApplicationMetadata(&cached_metadata);
        
        return true;
    }
    
    // 3. 缓存获取失败，尝试从NS服务获取（仅名称）
    // 3. Cache failed, try to get from NS service (name only)
    auto control_data = std::make_unique<NsApplicationControlData>();
    u64 jpeg_size{};
    
    Result result = nsGetApplicationControlData(NsApplicationControlSource_Storage, application_id, control_data.get(), sizeof(NsApplicationControlData), &jpeg_size);
    if (R_FAILED(result)) {
        return false;
    }
    
    int systemlanguage = tj::LangManager::getInstance().getCurrentLanguage();
    NacpLanguageEntry* language_entry = &control_data->nacp.lang[systemlanguage];
    
    // 如果当前语言条目为空，则遍历查找第一个非空条目
    // If the current language entry is empty, iterate to find the first non-empty entry
    if (language_entry->name[0] == '\0' && language_entry->author[0] == '\0') {
        for (int i = 0; i < 16; i++) {
            if (control_data->nacp.lang[i].name[0] != '\0' || control_data->nacp.lang[i].author[0] != '\0') {
                language_entry = &control_data->nacp.lang[i];
                break;
            }
        }
    }
    
    entry.name = language_entry->name;
    entry.id = application_id;
    entry.display_version = control_data->nacp.display_version; // 从NACP获取显示版本 (Get display version from NACP)
    // 暂时设置默认值，稍后异步加载
    // Set default values temporarily, load asynchronously later
    entry.image = this->default_icon_image;
    entry.own_image = false;
    
    // 缓存图标数据到AppEntry中，避免后续重复读取
    // Cache icon data in AppEntry to avoid repeated reads later
    if (jpeg_size > sizeof(NacpStruct)) {
        size_t icon_size = jpeg_size - sizeof(NacpStruct);
        entry.cached_icon_data.resize(icon_size);
        memcpy(entry.cached_icon_data.data(), control_data->icon, icon_size);
        entry.has_cached_icon = true;
        
        // 仍然添加到缓存系统以供其他用途，并传递版本信息
        // Still add to cache system for other uses, and pass version info
        nxtcAddEntry(application_id, &control_data->nacp, icon_size, control_data->icon, true, version_32);
    } else {
        entry.has_cached_icon = false;
    }
    
    return true;
}

// 专用于AddGame界面的应用信息获取函数
// Dedicated app info function for AddGame interface
bool App::TryGetAppBasicInfoWithIconCacheForAddGame(u64 application_id, AppEntry_AddGame& entry) {

    u32 version_32 = getgameversioninfo(application_id);

    // 2. entries中不存在，优先尝试从缓存获取应用名称
    // 2. Not found in entries, try to get application name from cache first
    NxTitleCacheApplicationMetadata* cached_metadata = nxtcGetApplicationMetadataEntryById(application_id);
    if (cached_metadata != nullptr && version_32 == cached_metadata->version_info) {
        entry.name = cached_metadata->name;
        entry.id = cached_metadata->title_id;
        entry.display_version = cached_metadata->version; 
        // 暂时设置默认值，稍后异步加载
        // Set default values temporarily, load asynchronously later
        entry.image = this->default_icon_image;
        entry.own_image = false;
        entry.unique_id = unique_id++;
        
        // 缓存图标数据到AppEntry_AddGame中
        // Cache icon data in AppEntry_AddGame
        if (cached_metadata->icon_data && cached_metadata->icon_size > 0) {
            entry.cached_icon_data.resize(cached_metadata->icon_size);
            memcpy(entry.cached_icon_data.data(), cached_metadata->icon_data, cached_metadata->icon_size);
            entry.has_cached_icon = true;
        } else {
            entry.has_cached_icon = false;
        }
        
        nxtcFreeApplicationMetadata(&cached_metadata);
        
        return true;
    }
    
    // 2. 缓存获取失败，尝试从NS服务获取（仅名称）
    // 2. Cache failed, try to get from NS service (name only)
    auto control_data = std::make_unique<NsApplicationControlData>();
    u64 jpeg_size{};
    
    Result result = nsGetApplicationControlData(NsApplicationControlSource_Storage, application_id, control_data.get(), sizeof(NsApplicationControlData), &jpeg_size);
    if (R_FAILED(result)) {
        return false;
    }
    
    int systemlanguage = tj::LangManager::getInstance().getCurrentLanguage();
    NacpLanguageEntry* language_entry = &control_data->nacp.lang[systemlanguage];
    
    // 如果当前语言条目为空，则遍历查找第一个非空条目
    // If the current language entry is empty, iterate to find the first non-empty entry
    if (language_entry->name[0] == '\0' && language_entry->author[0] == '\0') {
        for (int i = 0; i < 16; i++) {
            if (control_data->nacp.lang[i].name[0] != '\0' || control_data->nacp.lang[i].author[0] != '\0') {
                language_entry = &control_data->nacp.lang[i];
                break;
            }
        }
    }
    
    entry.name = language_entry->name;
    entry.id = application_id;
    entry.display_version = control_data->nacp.display_version; // 从NACP获取显示版本 (Get display version from NACP)
    // 暂时设置默认值，稍后异步加载
    // Set default values temporarily, load asynchronously later
    entry.image = this->default_icon_image;
    entry.own_image = false;
    entry.unique_id = unique_id++;
    
    // 缓存图标数据到AppEntry_AddGame中，避免后续重复读取
    // Cache icon data in AppEntry_AddGame to avoid repeated reads later
    if (jpeg_size > sizeof(NacpStruct)) {
        size_t icon_size = jpeg_size - sizeof(NacpStruct);
        entry.cached_icon_data.resize(icon_size);
        memcpy(entry.cached_icon_data.data(), control_data->icon, icon_size);
        entry.has_cached_icon = true;
        
        // 仍然添加到缓存系统以供其他用途
        // Still add to cache system for other uses
        nxtcAddEntry(application_id, &control_data->nacp, icon_size, control_data->icon, true, version_32);
    } else {
        entry.has_cached_icon = false;
    }
    
    return true;
}

// 辅助函数：根据filename_path获取其下面的modid和mod目录数量（使用标准C库）
// Helper function: Get modid and mod directory count under filename_path (using standard C library)
std::pair<u64, std::string> App::GetModIdAndSubdirCountStdio(const std::string& filename_path) {
    // 打开filename目录
    // Open filename directory
    DIR* filename_dir = opendir(filename_path.c_str());
    if (!filename_dir) {
        return {0, "0"}; // 返回空结果
    }
    
    // 读取第一个有效目录条目（应该是唯一的modid文件夹）
    // Read the first valid directory entry (should be the only modid folder)
    struct dirent* entry;
    u64 application_id = 0;
    std::string modid_name; // 保存modid目录名，避免在closedir后访问失效指针
    
    while ((entry = readdir(filename_dir)) != nullptr) {
        // 跳过当前目录和父目录
        // Skip current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 将modid字符串转换为u64类型的应用ID
        // Convert modid string to u64 application ID
        char* endptr;
        application_id = strtoull(entry->d_name, &endptr, 16);
        
        // 检查转换是否成功（modid应该是16进制字符串）
        // Check if conversion was successful (modid should be hex string)
        if (*endptr == '\0' && application_id != 0) {
            modid_name = entry->d_name; // 保存有效的modid目录名
            break;
        }
    }
    
    closedir(filename_dir);
    
    if (application_id == 0 || modid_name.empty()) {
        return {0, "0"}; // 未找到有效的modid
    }
    
    // 读取modid目录下的子目录数量
    // Read subdirectory count under modid directory
    std::string modid_path = filename_path + "/" + modid_name;
    DIR* modid_dir = opendir(modid_path.c_str());
    
    int mod_count = 0; // MOD目录数量计数器
    if (modid_dir) {
        struct dirent* subentry;
        while ((subentry = readdir(modid_dir)) != nullptr) {
            // 跳过当前目录和父目录
            // Skip current and parent directory entries
            if (strcmp(subentry->d_name, ".") == 0 || strcmp(subentry->d_name, "..") == 0) {
                continue;
            }
            
            // 判断类型
            if (subentry->d_type == DT_DIR) {
                // 是目录，直接计数
                mod_count++;
            } else if (subentry->d_type == DT_REG) {
                // 是文件，检查是否为zip
                std::string filename = subentry->d_name;
                size_t len = filename.length();
                if (len > 4) {
                    std::string ext = filename.substr(len - 4);
                    if (ext == ".zip" || ext == ".ZIP") {
                        mod_count++;
                    }
                }
            }
            // 其他类型（链接、特殊文件等）不处理
        }
        closedir(modid_dir);
    }
    
    return {application_id, std::to_string(mod_count)};
}





void App::FastScanModInfo() {
    // 清空之前的MOD信息
    // Clear previous MOD information
    this->mod_info.clear();
    
    // 获取当前选中游戏的文件路径和MOD版本（加锁保护读取操作）
    // Get filepath and MOD version of currently selected game (with lock protection for read operation)
   
    
    std::string mod_path = this->entries[this->index].FILE_PATH;
    // 打开MOD目录
    // Open MOD directory
    DIR* mod_dir = opendir(mod_path.c_str());
    if (!mod_dir) {
        return; // 无法打开目录，直接返回 (Cannot open directory, return directly)
    }

    struct dirent* entry;
    // 遍历MOD目录下的所有条目
    // Iterate through all entries in MOD directory
    while ((entry = readdir(mod_dir)) != nullptr) {
        // 跳过当前目录和父目录
        // Skip current and parent directory entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 跳过文件，只处理目录
        // Skip files, only process directories
        if (entry->d_type == DT_REG) {
            continue; // 跳过普通文件 (Skip regular files)
        }
        
        std::string dirname = entry->d_name;
        
        // 创建MODINFO结构体并解析目录名称
        // Create MODINFO structure and parse directory name
        MODINFO mod_info_item;

        mod_info_item.MOD_PATH = mod_path + "/" + dirname;
        
        // 检查末尾是否有$符号来判断安装状态
        // Check if there's a $ symbol at the end to determine installation status
        if (!dirname.empty() && dirname.back() == '$') {
            mod_info_item.MOD_STATE = true;
            dirname.pop_back(); // 移除末尾的$符号 (Remove the $ symbol at the end)
        } else {
            mod_info_item.MOD_STATE = false; // 设置安装状态 (Set installation state)
        }
        
        // 解析目录名称格式：XXXX[X] 或 XXXX
        // Parse directory name format: XXXX[X] or XXXX
        size_t bracket_start = dirname.find('[');
        size_t bracket_end = dirname.find(']');
        
        if (bracket_start != std::string::npos && bracket_end != std::string::npos && bracket_end > bracket_start) {
            // XXXX[X] 格式：XXXX存储到MOD_NAME，[X]存储到MOD_TYPE
            // XXXX[X] format: XXXX to MOD_NAME, [X] to MOD_TYPE
            mod_info_item.MOD_NAME = dirname.substr(0, bracket_start);
            mod_info_item.MOD_TYPE = dirname.substr(bracket_start, bracket_end - bracket_start + 1);
        } else {
            // XXXX 格式：XXXX存储到MOD_NAME，MOD_TYPE为空
            // XXXX format: XXXX to MOD_NAME, MOD_TYPE empty
            mod_info_item.MOD_NAME = dirname;
            mod_info_item.MOD_TYPE = NONE_TYPE_TEXT;
        }
        mod_info_item.MOD_NAME2 = GetMappedModName(mod_info_item.MOD_NAME, mod_info_item.MOD_TYPE);
        // 将解析的MOD信息添加到容器中
        // Add parsed MOD information to container
        this->mod_info.push_back(mod_info_item);
    }
    
    // 关闭目录
    // Close directory
    closedir(mod_dir);
}





// 提前收集并排序好主页需要扫描的目录
std::vector<std::string> App::scanmodgamedir() {

    std::vector<std::string> dir_paths; 
    
    // 如果打不开直接返回空数组
    DIR* dir = opendir("/mods2/"); 
    if (dir == nullptr) {
        
        return dir_paths;
    }
    
    struct dirent* entry;

    // 开始遍历目录
    while ((entry = readdir(dir)) != nullptr) {
        
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 过滤掉非目录，目录名保存到这个数组
        if (entry->d_type == DT_DIR) {
            dir_paths.push_back(std::string(entry->d_name)); 
        }
    }
    
    // 扫描完毕关闭目录
    closedir(dir); 

    
    // 这个是对扫描到的目录dir_paths进行去重，筛选出josn文件中没有的目录

    std::unordered_set<std::string> existing_games;
    
    for (const auto& game_dir : favorites_games_array) {
        if (!game_dir.empty()) {
            existing_games.insert(game_dir); 
        }
    }
    
    for (const auto& game_dir : non_favorites_games_array) {
        if (!game_dir.empty()) {
            existing_games.insert(game_dir); 
        }
    }
    
    dir_paths.erase(
        std::remove_if(dir_paths.begin(), dir_paths.end(),
            [&existing_games](const std::string& dir_name) {
                return existing_games.find(dir_name) != existing_games.end();
            }),
        dir_paths.end()
    );


    // 这里对JOSN中收集到的目录进行排序，按映射名的拼音排序
    std::ranges::sort(non_favorites_games_array, [this](const std::string& game_dir_a, const std::string& game_dir_b) {
        return gamedirsort(game_dir_a, game_dir_b);
    });
    
    
    std::ranges::sort(favorites_games_array, [this](const std::string& game_dir_a, const std::string& game_dir_b) {
        return gamedirsort(game_dir_a, game_dir_b);
    });

    // 排序完成后，在favorites_games_array最后添加一个END$180作为标记
    favorites_games_array.push_back("END::FAVORITES");
    
    // 合并三个数组，这个最终的数组就是我们要最终要扫描的目录
    std::vector<std::string> merged_games;
    
    merged_games.insert(merged_games.end(), 
                             favorites_games_array.begin(), 
                             favorites_games_array.end());
    
    merged_games.insert(merged_games.end(), 
                             non_favorites_games_array.begin(), 
                             non_favorites_games_array.end());
    
    merged_games.insert(merged_games.end(), 
                             dir_paths.begin(), 
                             dir_paths.end());
    
    return merged_games; 
}


// 分离式扫描：第一阶段快速扫描应用名称和大小
// Separated scanning: Phase 1 - Fast scan application names
void App::FastScanNames(std::stop_token stop_token) {
    // 标记扫描开始
    is_scan_running = true;

    // 初始化扫描计数器
    scanned_count = 0;

    // 计数器，用于统计找到的应用总数
    size_t count{};

    // 初始化libnxtc库
    if (!nxtcInitialize()) {

        // 标记扫描结束
        is_scan_running = false;

        // 标记扫描完成
        this->finished_scanning = true;

        return;
        
    }

    Result rc = avmInitialize();
    if (R_FAILED(rc)) {

        // 标记扫描结束
        is_scan_running = false;

        // 刷新缓存文件
        nxtcFlushCacheFile();

        // 退出libnxtc库
        nxtcExit();

        // 标记扫描完成
        this->finished_scanning = true;

        return;

    }

    // 获取需要扫描的目录名字
    std::vector<std::string> gamedirname = scanmodgamedir();

    bool is_favorite = true;

    // 直接遍历gamedirname数组中的目录名
    // Directly iterate through directory names in gamedirname array
    for (const std::string& dirname : gamedirname) {
        if (stop_token.stop_requested()) {
            avmExit();
            nxtcExit();
            break;
        }

        if (dirname == "END::FAVORITES") {
            is_favorite = false;
            continue;
        }
        
        // 分割文件名，提取应用名称和版本号
        // Split filename to extract app name and version
        std::string full_name = dirname;
        std::string FILE_NAME;
        std::string MOD_VERSION;
        
        // 查找方括号的位置
        // Find bracket positions
        size_t bracket_start = full_name.find('[');
        size_t bracket_end = full_name.find(']');
        
        if (bracket_start != std::string::npos && bracket_end != std::string::npos && bracket_end > bracket_start) {
            // 格式: XXXX[1.0.2]
            // Format: XXXX[1.0.2]
            FILE_NAME = full_name.substr(0, bracket_start);
            MOD_VERSION = full_name.substr(bracket_start + 1, bracket_end - bracket_start - 1);
        } else {
            // 格式: XXXX
            // Format: XXXX
            FILE_NAME = full_name;
            MOD_VERSION = NONE_TYPE_TEXT;
        }
        // 构建filename文件夹的完整路径
        // Build full path for filename folder
        std::string filename_path = "/mods2/" + dirname;
        
        // 获取当前filename目录下的modid和mod目录数量
        // Get modid and mod directory count under current filename directory
        std::pair<u64, std::string> modid_count_result = GetModIdAndSubdirCountStdio(filename_path);
        
        // 从返回的pair中提取application_id和mod目录数量字符串
        // Extract application_id and mod directory count string from returned pair
        u64 application_id = modid_count_result.first;
        std::string mod_count = modid_count_result.second;
        
        // 如果没有找到有效的modid，跳过当前目录
        // Skip current directory if no valid modid found
        if (application_id == 0 || mod_count == "0") {
            continue;
        }

        char app_id[17]; // 16位十六进制字符串 + 结束符 (16-digit hex string + null terminator)
        snprintf(app_id, sizeof(app_id), "%016lX", application_id);

        // 应用条目，用于存储单个应用的信息
        AppEntry entry;
        bool is_corrupted = false;

        

        // 1. 快速获取应用名称和大小信息（不包含图标）
        // 1. Fast get application name and size info (excluding icon)
        if (!TryGetAppBasicInfoWithIconCache(application_id, entry)) {
            is_corrupted = true;
        }else{
            entry.FILE_NAME = FILE_NAME;
            entry.FILE_PATH = filename_path + "/" + app_id;
            entry.MOD_TOTAL = mod_count;
            entry.MOD_VERSION = MOD_VERSION;
            entry.FILE_NAME2 = GetMappedGameName(FILE_NAME, MOD_VERSION);
            entry.unique_id = unique_id++;
            entry.is_favorite = is_favorite;
        }
        
        // 标记是否存在损坏的安装
        // Mark if there is a corrupted installation
        if (is_corrupted) {
            // 损坏的安装处理
            // Handle corrupted installation
            entry.name = FILE_NAME;
            entry.FILE_NAME = FILE_NAME;
            entry.FILE_PATH = filename_path + "/" + app_id;
            entry.display_version = NONE_GAME_TEXT;
            entry.id = application_id;
            entry.image = this->default_icon_image;
            entry.own_image = false;
            // 损坏安装的模组数量显示为--
            // Show mod count as -- for corrupted installations
            entry.MOD_TOTAL = mod_count;
            entry.MOD_VERSION = MOD_VERSION;
            entry.FILE_NAME2 = GetMappedGameName(FILE_NAME, MOD_VERSION);
            entry.unique_id = unique_id++;
            entry.is_favorite = is_favorite;
        }

        // 加锁保护entries列表
        {
            std::scoped_lock lock{entries_mutex};
            // 将应用条目添加到列表
            this->entries.emplace_back(std::move(entry));
            // 更新扫描计数器
            scanned_count++;
            count++;
        }

        // 为首屏4个应用立即提交最高优先级图标加载任务
        // Immediately submit highest priority icon loading tasks for first screen 4 apps
        if (count <= BATCH_SIZE && !is_corrupted && entry.has_cached_icon) {
            ResourceLoadTask icon_task;
            icon_task.unique_id = entry.unique_id;
            icon_task.priority = 0; // 最高优先级 (Highest priority)
            icon_task.submit_time = std::chrono::steady_clock::now();
            icon_task.task_type = ResourceTaskType::ICON;
            
            icon_task.load_callback = [this, unique_id = entry.unique_id]() {
                std::vector<unsigned char> icon_data;
                bool has_icon_data = false;
                
                // 获取缓存的图标数据
                // Get cached icon data
                {
                    std::scoped_lock lock{entries_mutex};
                    auto it = std::find_if(entries.begin(), entries.end(),
                        [unique_id](const AppEntry& entry) {
                            return entry.unique_id == unique_id && entry.has_cached_icon;
                        });
                    
                    if (it != entries.end()) {
                        icon_data = it->cached_icon_data;
                        has_icon_data = true;
                    }
                }
                
                // 验证并创建图像
                // Validate and create image
                if (has_icon_data && !icon_data.empty() && IsValidJpegData(icon_data)) {
                    int image_id = nvgCreateImageMem(this->vg, 0, icon_data.data(), icon_data.size());
                    if (image_id > 0) {
                        std::scoped_lock lock{entries_mutex};
                        auto it = std::find_if(entries.begin(), entries.end(),
                            [unique_id](const AppEntry& entry) {
                                return entry.unique_id == unique_id;
                            });
                        
                        if (it != entries.end()) {
                            // 如果之前有自己的图像，先删除
                            // If previously had own image, delete it first
                            if (it->own_image && it->image != this->default_icon_image) {
                                nvgDeleteImage(this->vg, it->image);
                            }
                            it->image = image_id;
                            it->own_image = true;
                        }
                    }
                }
            };
            
            resource_manager.submitLoadTask(icon_task);
        }

        // 首批应用名称加载完成标记
        // Mark first batch names loading complete
        if (scanned_count.load() == BATCH_SIZE) {
            initial_batch_loaded = true;
        }

        //线程休息1ms避免卡死（比原来更快）
        svcSleepThread(1000000ULL);
    }

done:
    // 标记扫描结束
    is_scan_running = false;

    // 刷新缓存文件
    nxtcFlushCacheFile();

    // 退出libnxtc库
    nxtcExit();

    avmExit();

    // 加锁保护finished_scanning
    std::scoped_lock lock{this->mutex};

    // 标记扫描完成
    this->finished_scanning = true;

}

// 专用异步扫描函数：扫描设备上所有游戏
// Dedicated async scan function: scan all games on device
void App::FastScanAllGames(std::stop_token stop_token) {
    // 标记ADDGAMELIST扫描开始
    // Mark ADDGAMELIST scanning start
    addgame_scan_running = true;
    addgame_scanned_count = 0;
    
    // 获取所有应用ID
    // Get all application IDs
    std::vector<u64> app_ids;
    Result result = GetAllApplicationIds(app_ids);
    if (R_FAILED(result)) {
        addgame_scan_running = false; // 获取失败，标记扫描结束 (Failed to get, mark scanning end)
        return; // 获取失败，直接返回 (Failed to get, return directly)
    }
    
    // 设置ADDGAMELIST总数
    // Set ADDGAMELIST total count
    addgame_total_count = app_ids.size();

    // 初始化libnxtc库
    // Initialize libnxtc library
    if (!nxtcInitialize())  {
        addgame_scan_running = false; // 初始化失败，标记扫描结束 (Failed to initialize, mark scanning end)
        return; // 初始化失败，直接返回 (Failed to initialize, return directly)
    }

    Result rc = avmInitialize();
    if (R_FAILED(rc)) {
        addgame_scan_running = false;
        return; // AVM服务初始化失败，直接返回 (AVM service initialization failed, return directly)
    }

    size_t count = 0;
    
    // 遍历所有应用ID
    // Iterate through all application IDs
    for (u64 application_id : app_ids) {
        if (stop_token.stop_requested()) {
            avmExit();
            nxtcExit();
            break; // 如果请求停止，退出循环 (If stop requested, exit loop)
        }

        // 应用条目，用于存储单个应用的信息
        // Application entry for storing single application info
        AppEntry_AddGame entry;
        bool is_corrupted = false;

        // 使用专用函数快速获取应用基本信息并缓存图标数据
        // Use dedicated function to fast get application basic info and cache icon data
        if (!TryGetAppBasicInfoWithIconCacheForAddGame(application_id, entry)) {
            // 获取失败，标记为损坏的安装
            // Failed to get info, mark as corrupted installation
            is_corrupted = true;
            entry.name = "Unknown Game";
            entry.id = application_id;
            entry.display_version = "Unknown";
            entry.image = this->default_icon_image;
            entry.own_image = false;
            entry.has_cached_icon = false;
            entry.unique_id = unique_id++;
        }

        // 保存entry的has_cached_icon状态，因为entry即将被移动
        // Save entry's has_cached_icon status since entry will be moved
        bool has_cached_icon = entry.has_cached_icon;
        
        // 加锁保护entries_AddGame列表
        // Lock to protect entries_AddGame list
        {
            std::scoped_lock lock{entries_AddGame_mutex};
            // 将应用条目添加到列表
            // Add application entry to list
            this->entries_AddGame.emplace_back(entry);
            count++;
        }
        
        // 更新ADDGAMELIST扫描计数
        // Update ADDGAMELIST scan count
        addgame_scanned_count++;

        // 为首屏4个应用立即提交最高优先级图标加载任务
        // Immediately submit highest priority icon loading tasks for first screen 4 apps
        if (count <= BATCH_SIZE && !is_corrupted && has_cached_icon) {
            ResourceLoadTask icon_task;
            icon_task.unique_id = entry.unique_id;
            icon_task.priority = 0; // 最高优先级 (Highest priority)
            icon_task.submit_time = std::chrono::steady_clock::now();
            icon_task.task_type = ResourceTaskType::ICON;
            
            icon_task.load_callback = [this, unique_id = entry.unique_id]() {
                std::vector<unsigned char> icon_data;
                bool has_icon_data = false;
                
                // 获取缓存的图标数据
                // Get cached icon data
                {
                    std::scoped_lock lock{entries_AddGame_mutex};
                    auto it = std::find_if(entries_AddGame.begin(), entries_AddGame.end(),
                        [unique_id](const AppEntry_AddGame& entry) {
                            return entry.unique_id == unique_id && entry.has_cached_icon;
                        });
                    
                    if (it != entries_AddGame.end()) {
                        icon_data = it->cached_icon_data;
                        has_icon_data = true;
                    }
                }
                
                // 验证并创建图像
                // Validate and create image
                if (has_icon_data && !icon_data.empty() && IsValidJpegData(icon_data)) {
                    int image_id = nvgCreateImageMem(this->vg, 0, icon_data.data(), icon_data.size());
                    if (image_id > 0) {
                        std::scoped_lock lock{entries_AddGame_mutex};
                        auto it = std::find_if(entries_AddGame.begin(), entries_AddGame.end(),
                            [unique_id](const AppEntry_AddGame& entry) {
                                return entry.unique_id == unique_id;
                            });
                        
                        if (it != entries_AddGame.end()) {
                            // 如果之前有自己的图像，先删除
                            // If previously had own image, delete it first
                            if (it->own_image && it->image != this->default_icon_image) {
                                nvgDeleteImage(this->vg, it->image);
                            }
                            it->image = image_id;
                            it->own_image = true;
                        }
                    }
                }
            };
            
            resource_manager.submitLoadTask(icon_task);
        }

        // 线程休息1ms避免卡死
        // Thread sleep 1ms to avoid freezing
        svcSleepThread(1000000ULL);
    }
    
    // 刷新缓存文件
    nxtcFlushCacheFile();
    // 退出libnxtc库
    // Exit libnxtc library
    nxtcExit();
    avmExit();
    // 标记ADDGAMELIST扫描完成
    // Mark ADDGAMELIST scanning complete
    addgame_scan_running = false;
}


// 计算当前可见区域的应用索引范围
// Calculate the index range of applications in current visible area
std::pair<size_t, size_t> App::GetVisibleRange() const {
    std::scoped_lock lock{entries_mutex};
    
    if (entries.empty()) {
        return {0, 0};
    }
    
    // 九宫格布局：每页显示9个应用项 (Grid layout: 9 application items per page)
    constexpr size_t ITEMS_PER_PAGE = 9;
    
    // 计算当前页的起始和结束索引 (Calculate start and end index for current page)
    const size_t current_page = this->index / ITEMS_PER_PAGE;
    const size_t visible_start = current_page * ITEMS_PER_PAGE;
    const size_t visible_end = std::min(visible_start + ITEMS_PER_PAGE, entries.size());
    
    return {visible_start, visible_end};
}

// 获取列表界面的可见范围 (Get visible range for list interface)


// 基于视口的智能图标加载：根据光标位置优先加载可见区域的图标
// Viewport-aware smart icon loading: prioritize loading icons in visible area based on cursor position

void App::LoadVisibleAreaIcons() {
    // 如果应用列表为空，直接返回，避免没有应用的设备出现问题
    // If application list is empty, return directly to avoid issues on devices without apps
    {
        std::scoped_lock lock{entries_mutex};
        if (entries.empty()) {
            return;
        }
    }

    auto now = std::chrono::steady_clock::now();
    
    // 防抖：如果距离上次调用不足100ms，则跳过
    // Debouncing: skip if less than 100ms since last call
    constexpr auto DEBOUNCE_INTERVAL = std::chrono::milliseconds(100);
    if (now - last_load_time < DEBOUNCE_INTERVAL) {
        return;
    }
    last_load_time = now;
    
    auto [visible_start, visible_end] = GetVisibleRange();
    
    // 如果可见区域没有变化且不是强制重置状态，则跳过
    // Skip if visible range hasn't changed and not in force reset state
    if (last_loaded_range.first == visible_start && last_loaded_range.second == visible_end && 
        last_loaded_range.first != SIZE_MAX) {
        return;
    }
    last_loaded_range = {visible_start, visible_end};
    
    // 图标加载策略：加载当前屏幕4个应用和下方即将进入屏幕的应用
    // Icon loading strategy: load current screen 4 apps and upcoming apps below
    constexpr size_t PRELOAD_BUFFER = 3; // 下方预加载的应用数量
    constexpr size_t FIRST_SCREEN_SIZE = 4; // 首屏应用数量
    
    size_t load_start = visible_start; // 从当前可见区域开始，不预加载上方
    size_t load_end;
    {
        std::scoped_lock lock{entries_mutex};
        load_end = std::min(visible_end + PRELOAD_BUFFER, entries.size());
    }
    
    // 批量收集需要加载图标的应用信息，减少锁的使用
    // Batch collect applications that need icon loading to reduce lock usage
    struct LoadInfo {
        size_t unique_id;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
        int priority;
    };
    
    std::vector<LoadInfo> load_infos;
    load_infos.reserve(load_end - load_start);
    
    {
        std::scoped_lock lock{entries_mutex};
        // 确保索引范围有效
        // Ensure index range is valid
        const size_t actual_end = std::min(load_end, entries.size());
        
        for (size_t i = load_start; i < actual_end; ++i) {
            const auto& entry = entries[i];
            
            // 优化：同时检查图标状态和损坏状态，减少后续处理
            // Optimization: check both icon status and corruption status to reduce subsequent processing
            if (entry.image == this->default_icon_image && entry.display_version != NONE_GAME_TEXT) {
                LoadInfo info;
                info.unique_id = entry.unique_id;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
                
                // 优先级策略：首屏4个应用最高优先级(0)，当前可见区域次高优先级(1)，其他为低优先级(2)
                // Priority strategy: first screen 4 apps highest priority(0), current visible area medium priority(1), others low priority(2)
                if (i < FIRST_SCREEN_SIZE) {
                    info.priority = 0; // 首屏应用最高优先级
                } else if (i >= visible_start && i < visible_end) {
                    info.priority = 1; // 当前可见区域次高优先级
                } else {
                    info.priority = 2; // 其他应用低优先级
                }
                
                load_infos.push_back(std::move(info));
            }
        }
    }
    
    // 如果没有需要加载的图标，直接返回
    // Return early if no icons need loading
    if (load_infos.empty()) {
        return;
    }
    
    // 处理收集到的加载信息
    // Process collected loading information
    for (const auto& info : load_infos) {
        
        // 提交图标加载任务
        // Submit icon loading task
        ResourceLoadTask icon_task;
        icon_task.unique_id = info.unique_id;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
        icon_task.priority = info.priority;
        icon_task.submit_time = std::chrono::steady_clock::now();
        icon_task.task_type = ResourceTaskType::ICON; // 标记为图标任务 (Mark as icon task)
        
        icon_task.load_callback = [this, unique_id = info.unique_id]() {
            // 获取图标数据
            // Get icon data
            std::vector<unsigned char> icon_data;
            bool has_icon_data = false;
            
            // 优先使用AppEntry中缓存的图标数据，避免重复的缓存读取
            // Prioritize using cached icon data in AppEntry to avoid repeated cache reads
            {
                std::scoped_lock lock{entries_mutex};
                auto it = std::find_if(entries.begin(), entries.end(),
                    [unique_id](const AppEntry& entry) {
                        return entry.unique_id == unique_id && entry.has_cached_icon;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
                    });
                
                if (it != entries.end()) {
                    icon_data = it->cached_icon_data;
                    has_icon_data = true;
                }
            }
            
            
             if (!has_icon_data) {
                 return; // 没有可用的图标数据，直接返回
             }
            
            // 验证并创建图像
            // Validate and create image
            if (has_icon_data && !icon_data.empty() && IsValidJpegData(icon_data)) {
                int image_id = nvgCreateImageMem(this->vg, 0, icon_data.data(), icon_data.size());
                if (image_id > 0) {
                    std::scoped_lock lock{entries_mutex};
                    auto it = std::find_if(entries.begin(), entries.end(),
                        [unique_id](const AppEntry& entry) {
                            return entry.unique_id == unique_id;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
                        });
                    
                    if (it != entries.end()) {
                        // 如果之前有自己的图像，先删除
                        // If previously had own image, delete it first
                        if (it->own_image && it->image != this->default_icon_image) {
                            nvgDeleteImage(this->vg, it->image);
                        }
                        it->image = image_id;
                        it->own_image = true;
                    }
                }
            }
        };
        
        this->resource_manager.submitLoadTask(icon_task);
    }
}

// 卸载界面的可见区域图标加载 (Visible area icon loading for uninstall interface)
// 只加载当前屏幕可见的应用图标和下方2个预加载图标，避免内存浪费
// Only load icons for currently visible apps and 2 preload icons below, avoiding memory waste


// 加载卸载界面的图标 (Load icons for uninstall interface)
// 为列表界面中的应用加载图标 (Load icons for apps in list interface)

void App::LoadAddGameVisibleAreaIcons() {
    // 如果AddGame应用列表为空，直接返回，避免没有应用的设备出现问题
    // If AddGame application list is empty, return directly to avoid issues on devices without apps
    {
        std::scoped_lock lock{entries_AddGame_mutex};
        if (entries_AddGame.empty()) {
            return;
        }
    }

    auto now = std::chrono::steady_clock::now();
    
    // 防抖：如果距离上次调用不足100ms，则跳过
    // Debouncing: skip if less than 100ms since last call
    constexpr auto DEBOUNCE_INTERVAL = std::chrono::milliseconds(100);
    if (now - this->last_addgame_load_time < DEBOUNCE_INTERVAL) {
        return;
    }
    this->last_addgame_load_time = now;
    
    // 计算九宫格布局的可见范围 (Calculate visible range for grid layout)
    const int GRID_COLS = 3; // 九宫格列数 (Grid columns)
    const int GRID_ROWS = 3; // 九宫格行数 (Grid rows)
    const int ITEMS_PER_PAGE = GRID_COLS * GRID_ROWS; // 每页项目数 (Items per page)
    
    size_t current_page = this->index_AddGame / ITEMS_PER_PAGE;
    size_t visible_start = current_page * ITEMS_PER_PAGE;
    size_t visible_end;
    {
        std::scoped_lock lock{entries_AddGame_mutex};
        visible_end = std::min(visible_start + ITEMS_PER_PAGE, entries_AddGame.size());
    }
    
    // 如果可见区域没有变化，则跳过
    // Skip if visible range hasn't changed
    if (this->last_addgame_loaded_range.first == visible_start && this->last_addgame_loaded_range.second == visible_end && 
        this->last_addgame_loaded_range.first != SIZE_MAX) {
        return;
    }
    this->last_addgame_loaded_range = {visible_start, visible_end};
    
    // 图标加载策略：加载当前页面和未来3个应用的图标
    // Icon loading strategy: load current page and next 3 apps icons
    constexpr size_t PRELOAD_BUFFER = 3; // 预加载未来3个应用的图标
    
    size_t load_start = visible_start;
    size_t load_end;
    {
        std::scoped_lock lock{entries_AddGame_mutex};
        load_end = std::min(visible_end + PRELOAD_BUFFER, entries_AddGame.size());
    }
    
    // 批量收集需要加载图标的应用信息，减少锁的使用
    // Batch collect applications that need icon loading to reduce lock usage
    struct LoadInfo {
        size_t unique_id;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
        int priority;
    };
    
    std::vector<LoadInfo> load_infos;
    load_infos.reserve(load_end - load_start);
    
    {
        std::scoped_lock lock{entries_AddGame_mutex};
        // 确保索引范围有效
        // Ensure index range is valid
        const size_t actual_end = std::min(load_end, entries_AddGame.size());
        
        for (size_t i = load_start; i < actual_end; ++i) {
            const auto& entry = entries_AddGame[i];
            
            // 检查是否需要加载图标：图标为默认图标且有缓存数据
            // Check if icon loading is needed: icon is default and has cached data
            if (entry.image == this->default_icon_image && entry.has_cached_icon) {
                LoadInfo info;
                info.unique_id = entry.unique_id;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
                
                // 优先级策略：当前可见页面最高优先级(0)，预加载页面低优先级(1)
                // Priority strategy: current visible page highest priority(0), preload page low priority(1)
                if (i >= visible_start && i < visible_end) {
                    info.priority = 0; // 当前可见页面最高优先级
                } else {
                    info.priority = 1; // 预加载页面低优先级
                }
                
                load_infos.push_back(std::move(info));
            }
        }
    }
    
    // 如果没有需要加载的图标，直接返回
    // Return early if no icons need loading
    if (load_infos.empty()) {
        return;
    }
    
    // 处理收集到的加载信息
    // Process collected loading information
    for (const auto& info : load_infos) {
        
        // 提交图标加载任务
        // Submit icon loading task
        ResourceLoadTask icon_task;
        icon_task.unique_id = info.unique_id;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
        icon_task.priority = info.priority;
        icon_task.submit_time = std::chrono::steady_clock::now();
        icon_task.task_type = ResourceTaskType::ICON; // 标记为图标任务 (Mark as icon task)
        
        icon_task.load_callback = [this, unique_id = info.unique_id]() {
            // 获取图标数据
            // Get icon data
            std::vector<unsigned char> icon_data;
            bool has_icon_data = false;
            
            // 优先使用AppEntry_AddGame中缓存的图标数据，避免重复的缓存读取
            // Prioritize using cached icon data in AppEntry_AddGame to avoid repeated cache reads
            {
                std::scoped_lock lock{entries_AddGame_mutex};
                auto it = std::find_if(entries_AddGame.begin(), entries_AddGame.end(),
                    [unique_id](const AppEntry_AddGame& entry) {
                        return entry.unique_id == unique_id && entry.has_cached_icon;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
                    });
                
                if (it != entries_AddGame.end()) {
                    icon_data = it->cached_icon_data;
                    has_icon_data = true;
                }
            }
            
            
             if (!has_icon_data) {
                 return; // 没有可用的图标数据，直接返回
             }
            
            // 验证并创建图像
            // Validate and create image
            if (has_icon_data && !icon_data.empty() && IsValidJpegData(icon_data)) {
                int image_id = nvgCreateImageMem(this->vg, 0, icon_data.data(), icon_data.size());
                if (image_id > 0) {
                    std::scoped_lock lock{entries_AddGame_mutex};
                    auto it = std::find_if(entries_AddGame.begin(), entries_AddGame.end(),
                        [unique_id](const AppEntry_AddGame& entry) {
                            return entry.unique_id == unique_id;  // 使用unique_id替代application_id (Use unique_id instead of application_id)
                        });
                    
                    if (it != entries_AddGame.end()) {
                        // 如果之前有自己的图像，先删除
                        // If previously had own image, delete it first
                        if (it->own_image && it->image != this->default_icon_image) {
                            nvgDeleteImage(this->vg, it->image);
                        }
                        it->image = image_id;
                        it->own_image = true;
                    }
                }
            }
        };
        
        this->resource_manager.submitLoadTask(icon_task);
    }
}


App::App() {

    CheckMods2Path();

    // 使用封装后的方法自动加载系统语言
    // Automatically load the system language using lang_manager
    tj::LangManager::getInstance().loadSystemLanguage();

    
    
    
    PlFontData font_standard, font_extended, font_lang;
    
    // 加载默认的字体，用于显示拉丁文 
    // Load the default font for displaying Latin characters
    plGetSharedFontByType(&font_standard, PlSharedFontType_Standard);
    plGetSharedFontByType(&font_extended, PlSharedFontType_NintendoExt);

    

    // Create the deko3d device
    this->device = dk::DeviceMaker{}.create();

    // Create the main queue
    this->queue = dk::QueueMaker{this->device}.setFlags(DkQueueFlags_Graphics).create();

    // Create the memory pools
    this->pool_images.emplace(device, DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image, 16*1024*1024);
    this->pool_code.emplace(device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Code, 128*1024);
    this->pool_data.emplace(device, DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached, 1*1024*1024);

    // Create the static command buffer and feed it freshly allocated memory
    this->cmdbuf = dk::CmdBufMaker{this->device}.create();
    const CMemPool::Handle cmdmem = this->pool_data->allocate(this->StaticCmdSize);
    this->cmdbuf.addMemory(cmdmem.getMemBlock(), cmdmem.getOffset(), cmdmem.getSize());

    // Create the framebuffer resources
    this->createFramebufferResources();
    
    // 初始化动态命令缓冲区用于GPU命令优化
    // Initialize dynamic command buffers for GPU command optimization
    for (unsigned i = 0; i < NumCommandBuffers; ++i) {
        this->dynamic_cmdbufs[i] = dk::CmdBufMaker{this->device}.create();
        const CMemPool::Handle dynamic_cmdmem = this->pool_data->allocate(this->StaticCmdSize);
        this->dynamic_cmdbufs[i].addMemory(dynamic_cmdmem.getMemBlock(), dynamic_cmdmem.getOffset(), dynamic_cmdmem.getSize());
        
        // 初始化动态命令列表
        // Initialize dynamic command lists
        this->dynamic_cmdlists[i] = this->dynamic_cmdbufs[i].finishList();
        
        // 初始化同步对象
        // Initialize synchronization objects
        this->command_fences[i] = {};
    }

    this->renderer.emplace(1280, 720, this->device, this->queue, *this->pool_images, *this->pool_code, *this->pool_data);
    this->vg = nvgCreateDk(&*this->renderer, NVG_ANTIALIAS | NVG_STENCIL_STROKES);

    // 注册字体到NVG上下文
    // Register fonts to NVG context
    auto standard_font = nvgCreateFontMem(this->vg, "Standard", (unsigned char*)font_standard.address, font_standard.size, 0);
    auto extended_font = nvgCreateFontMem(this->vg, "Extended", (unsigned char*)font_extended.address, font_extended.size, 0);
    nvgAddFallbackFontId(this->vg, standard_font, extended_font);
    

    constexpr PlSharedFontType lang_font[] = {
        PlSharedFontType_ChineseSimplified,
        PlSharedFontType_ExtChineseSimplified,
        PlSharedFontType_ChineseTraditional,
        PlSharedFontType_KO,
    };


    for (auto type : lang_font) {
        if (R_SUCCEEDED(plGetSharedFontByType(&font_lang, type))) {
            char name[32];
            snprintf(name, sizeof(name), "Lang_%u", font_lang.type);
            auto lang_font = nvgCreateFontMem(this->vg, name, (unsigned char*)font_lang.address, font_lang.size, 0);
            nvgAddFallbackFontId(this->vg, standard_font, lang_font);
        } else {
            LOG("failed to load lang font %d\n", type);
        }
    }
    
    
    
    
    // 加载基础图像资源 (Load basic image resources)
    this->default_icon_image = nvgCreateImage(this->vg, "romfs:/default_icon.jpg", NVG_IMAGE_NEAREST);
    this->like_image = nvgCreateImage(this->vg, "romfs:/like.png", NVG_IMAGE_NEAREST);
    
    // MOD图标将在首次进入MODLIST时延迟加载 (MOD icons will be lazy loaded when first entering MODLIST)
    // 这样可以显著提升应用启动速度 (This significantly improves app startup speed)


    // 启动快速信息扫描
    // Start fast info scanning
    this->async_thread = util::async([this](std::stop_token stop_token){
            this->FastScanNames(stop_token);
            // 名称扫描完成后，初始视口加载由每帧调用自动处理
            // After name scanning is complete, initial viewport loading is handled by per-frame calls
        }
    );

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&this->pad);
    
    // 初始化触摸屏 (Initialize touch screen)
    hidInitializeTouchScreen();
    
    // 加载游戏名称映射 (Load game name mapping)
    LoadGameNameMapping();
   
    
    
    // 异步初始化音效管理器，避免阻塞启动流程 (Initialize audio manager asynchronously to avoid blocking startup)
    this->audio_manager.InitializeAsync();
    
    // 设置全局音效管理器指针 (Set global audio manager pointer)
    g_audio_manager = &this->audio_manager;
    
}

// 加载模组名称映射文件 (Load mod name mapping file)
bool App::LoadModNameMapping(const std::string& FILE_PATH) {
    // 清除之前的缓存 (Clear previous cache)
    mod_name_cache.clear();
    
    // 构建JSON文件路径，包含游戏ID (Build JSON file path, including game ID)
    
    std::string json_path = FILE_PATH + "/mod_name.json";
    
    // 尝试打开文件 (Try to open file)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) {
        return false; // 文件不存在，使用原始显示方式 (File doesn't exist, use original display)
    }
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return false;
    }
    
    // 读取文件内容 (Read file content)
    std::vector<char> buffer(file_size + 1);
    size_t read_size = fread(buffer.data(), 1, file_size, file);
    fclose(file);
    
    if (read_size != static_cast<size_t>(file_size)) {
        return false;
    }
    
    buffer[file_size] = '\0'; // 确保字符串结尾 (Ensure string termination)
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(buffer.data(), file_size, 0);
    if (!doc) {
        return false; // JSON解析失败 (JSON parsing failed)
    }
    
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }
    
    // 直接遍历根对象中的模组映射 (Directly iterate through mod mappings in root object)
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(root, &iter);
    yyjson_val* key, *val;
    
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = yyjson_obj_iter_get_val(key);
        if (!yyjson_is_obj(val)) continue;
        
        const char* mod_name = yyjson_get_str(key);
        if (!mod_name) continue;
        
        // 获取显示名称 (Get display name)
        yyjson_val* display_name_val = yyjson_obj_get(val, "display_name");
        const char* display_name = yyjson_get_str(display_name_val);
        
        // 获取描述 (Get description)
        yyjson_val* description_val = yyjson_obj_get(val, "description");
        const char* description = yyjson_get_str(description_val);
        
        // 存储到缓存中 (Store in cache)
        ModNameInfo info;
        info.display_name = display_name ? display_name : mod_name;
        info.description = description ? description : "";
        
        mod_name_cache[mod_name] = info;
    }
    
    yyjson_doc_free(doc);
    return true;
}

// 获取映射后的模组名称 (Get mapped mod name)
std::string App::GetMappedModName(const std::string& original_name, const std::string& mod_type) {
    std::unordered_map<std::string, ModNameInfo>::iterator it;
    if (mod_type == NONE_TYPE_TEXT) it = mod_name_cache.find(original_name);
    else it = mod_name_cache.find(original_name + mod_type);
    if (it != mod_name_cache.end()) {
        return it->second.display_name;
    }
    return original_name; // 未找到映射时返回原始名称 (Return original name if mapping not found)
}

// 获取模组描述 (Get mod description)
std::string App::GetModDescription(const std::string& original_name, const std::string& mod_type) {
    std::unordered_map<std::string, ModNameInfo>::iterator it;
    if (mod_type == NONE_TYPE_TEXT) it = mod_name_cache.find(original_name);
    else it = mod_name_cache.find(original_name + mod_type);
    if (it != mod_name_cache.end()) {
        return it->second.description;
    }
    return ""; // 未找到映射时返回空描述 (Return empty description if mapping not found)
}

// 清除模组名称缓存 (Clear mod name cache)
void App::ClearModNameCache() {
    mod_name_cache.clear();
}

// 加载游戏名称映射 (Load game name mapping)
bool App::LoadGameNameMapping() {
    // 构建game_name.json文件路径 (Build game_name.json file path)
    std::string json_path = "/mods2/game_name.json";
    
    
    // 打开文件 (Open file)
    FILE* file = fopen(json_path.c_str(), "rb");
    if (!file) {
        return false; // 文件不存在，使用原始显示方式 (File doesn't exist, use original display)
    }
    
    // 获取文件大小 (Get file size)
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fclose(file);
        return false;
    }
    
    
    // 读取文件内容 (Read file content)
    std::vector<char> buffer(file_size + 1);
    size_t read_size = fread(buffer.data(), 1, file_size, file);
    fclose(file);
    
    if (read_size != static_cast<size_t>(file_size)) {
        return false;
    }
    
    buffer[file_size] = '\0'; // 确保字符串结尾 (Ensure string termination)
    
    // 解析JSON (Parse JSON)
    yyjson_doc* doc = yyjson_read(buffer.data(), file_size, 0);
    if (!doc) {
        return false; // JSON解析失败 (JSON parsing failed)
    }
    
    yyjson_val* root = yyjson_doc_get_root(doc);
    if (!root || !yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }
    
    // 清空现有缓存和顺序列表 (Clear existing cache and order list)
    game_name_cache.clear();
    favorites_games_array.clear();     // 清空喜欢的游戏数组 (Clear favorites games array)
    non_favorites_games_array.clear(); // 清空非喜欢的游戏数组 (Clear non-favorites games array)
    
    // 遍历根对象中的游戏名称映射 (Iterate through game name mappings in root object)
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(root, &iter);
    yyjson_val* key, *val;
    
    int mapping_count = 0;
    while ((key = yyjson_obj_iter_next(&iter))) {
        val = yyjson_obj_iter_get_val(key);
        
        const char* game_name = yyjson_get_str(key);
        if (!game_name) continue;
        
        // 检查是否为favorites$180特殊对象 (Check if it's the special favorites$180 object)
        if (std::string(game_name) == "favorites$180") {
            // 处理favorites$180嵌套对象 (Process favorites$180 nested object)
            if (yyjson_is_obj(val)) {
                yyjson_obj_iter fav_iter;
                yyjson_obj_iter_init(val, &fav_iter);
                yyjson_val* fav_key, *fav_val;
                
                while ((fav_key = yyjson_obj_iter_next(&fav_iter))) {
                    fav_val = yyjson_obj_iter_get_val(fav_key);
                    
                    const char* fav_game_name = yyjson_get_str(fav_key);
                    const char* fav_display_name = yyjson_get_str(fav_val);
                    
                    if (fav_game_name && fav_display_name) {
                        // 存储到缓存中 (Store in cache)
                        ModNameInfo GAME_NAME;
                        GAME_NAME.display_name = fav_display_name;
                        
                        game_name_cache[fav_game_name] = GAME_NAME;
                        mapping_count++;
                        
                        // 添加到喜欢的游戏一维数组，只存储游戏目录名 (Add to favorites games 1D array, only store game directory name)
                        favorites_games_array.push_back(fav_game_name);
                    }
                }
            }
        } else {
            // 处理根级别的普通游戏映射 (Process root-level normal game mappings)
            const char* display_name = yyjson_get_str(val);
            if (display_name) {
                // 存储到缓存中 (Store in cache)
                ModNameInfo GAME_NAME;
                GAME_NAME.display_name = display_name;
                
                game_name_cache[game_name] = GAME_NAME;
                mapping_count++;
                
                // 添加到非喜欢的游戏一维数组，只存储游戏目录名 (Add to non-favorites games 1D array, only store game directory name)
                non_favorites_games_array.push_back(game_name);
            }
        }
    }
    
    
    yyjson_doc_free(doc);
    return true;
}

// 获取映射后的游戏名称 (Get mapped game name)
std::string App::GetMappedGameName(const std::string& original_name, const std::string& mod_version) {
    std::unordered_map<std::string, ModNameInfo>::iterator it;
    if (mod_version == NONE_TYPE_TEXT) it = game_name_cache.find(original_name);
    else it = game_name_cache.find(original_name + "[" + mod_version + "]");
    if (it != game_name_cache.end()) {
        return it->second.display_name;
    }
    return original_name; // 未找到映射时返回原始名称 (Return original name if mapping not found)
}


// 清空游戏名称映射缓存 (Clear game name mapping cache)
void App::ClearGameNameCache() {
    game_name_cache.clear();
    favorites_games_array.clear();     // 清空喜欢的游戏数组 (Clear favorites games array)
    non_favorites_games_array.clear(); // 清空非喜欢的游戏数组 (Clear non-favorites games array)
}

// 搜索过滤函数：根据输入的关键词匹配字符串数组，支持拼音搜索 (Search filter function: match string array based on input keywords, with pinyin support)
void App::FilterSearchResults(const std::string& search_text) {
    search_results.clear(); // 清空之前的搜索结果 (Clear previous search results)
    search_has_results = false;
    search_selected_index = 0;
    
    // 如果搜索文本为空，直接返回 (If search text is empty, return directly)
    if (search_text.empty()) {
        return;
    }
    
    // 将搜索文本转换为小写以进行不区分大小写的匹配 (Convert search text to lowercase for case-insensitive matching)
    std::string search_lower = search_text;
    std::transform(search_lower.begin(), search_lower.end(), search_lower.begin(), ::tolower);
    
    for (size_t idx = 0; idx < search_context.size(); ++idx) {
        // 使用search_context中的字符串作为搜索目标 (Use string from search_context as search target)
        const std::string& mapped_name = search_context[idx];
        bool matched = false;
        
        // 1. 直接匹配原始名称 (Direct match with original name)
        std::string name_lower = mapped_name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        if (name_lower.find(search_lower) != std::string::npos) {
            matched = true;
        }
        
        // 2. 拼音匹配：将中文转换为拼音进行匹配，支持多音字 (Pinyin matching: convert Chinese to pinyin for matching, support polyphonic characters)
        if (!matched) {
            // 存储所有可能的拼音组合 (Store all possible pinyin combinations)
            std::vector<std::string> all_pinyin_combinations;
            std::vector<std::string> all_initial_combinations;
            
            // 解析字符串中的每个字符，收集所有可能的拼音 (Parse each character in string, collect all possible pinyins)
            std::vector<std::vector<std::string>> char_pinyins; // 每个字符的所有拼音 (All pinyins for each character)
            std::vector<std::vector<char>> char_initials; // 每个字符的所有首字母 (All initials for each character)
            
            const char* str = mapped_name.c_str();
            size_t len = mapped_name.length();
            size_t i = 0;
            
            while (i < len) {
                wchar_t ch = 0;
                // 解析UTF-8字符 (Parse UTF-8 character)
                if ((str[i] & 0x80) == 0) {
                    // ASCII字符 (ASCII character)
                    ch = str[i];
                    i++;
                } else if ((str[i] & 0xE0) == 0xC0 && i + 1 < len) {
                    // 2字节UTF-8字符 (2-byte UTF-8 character)
                    ch = ((str[i] & 0x1F) << 6) | (str[i + 1] & 0x3F);
                    i += 2;
                } else if ((str[i] & 0xF0) == 0xE0 && i + 2 < len) {
                    // 3字节UTF-8字符 (3-byte UTF-8 character)
                    ch = ((str[i] & 0x0F) << 12) | ((str[i + 1] & 0x3F) << 6) | (str[i + 2] & 0x3F);
                    i += 3;
                } else {
                    // 跳过无效字符 (Skip invalid character)
                    i++;
                    continue;
                }
                
                if (WzhePinYin::Pinyin::IsChinese(ch)) {
                    // 获取中文字符的所有拼音 (Get all pinyins for Chinese character)
                    auto pinyins = WzhePinYin::Pinyin::GetPinyins(ch);
                    if (!pinyins.empty()) {
                        std::vector<std::string> current_pinyins;
                        std::vector<char> current_initials;
                        
                        // 处理所有拼音读音 (Process all pinyin pronunciations)
                        for (const auto& py : pinyins) {
                            std::string processed_py = py;
                            std::transform(processed_py.begin(), processed_py.end(), processed_py.begin(), ::tolower);
                            current_pinyins.push_back(processed_py);
                            if (!processed_py.empty()) {
                                current_initials.push_back(processed_py[0]);
                            }
                        }
                        
                        char_pinyins.push_back(current_pinyins);
                        char_initials.push_back(current_initials);
                    }
                } else {
                    // 非中文字符：忽略标点符号，只添加字母和数字 (Non-Chinese characters: ignore punctuation, only add letters and numbers)
                    if (ch < 128) { // 只处理ASCII字符 (Only handle ASCII characters)
                        char ascii_ch = static_cast<char>(ch);
                        if (std::isalnum(ascii_ch)) { // 只添加字母和数字，忽略标点符号 (Only add alphanumeric, ignore punctuation)
                            char_pinyins.push_back({std::string(1, ascii_ch)});
                            char_initials.push_back({ascii_ch});
                        }
                    }
                }
            }
            
            // 生成所有可能的拼音组合 (Generate all possible pinyin combinations)
            std::function<void(size_t, std::string, std::string)> generate_combinations = 
                [&](size_t pos, std::string current_full, std::string current_initials) {
                    if (pos >= char_pinyins.size()) {
                        if (!current_full.empty()) {
                            all_pinyin_combinations.push_back(current_full);
                        }
                        if (!current_initials.empty()) {
                            all_initial_combinations.push_back(current_initials);
                        }
                        return;
                    }
                    
                    // 为当前位置的每个拼音生成组合 (Generate combinations for each pinyin at current position)
                    for (size_t j = 0; j < char_pinyins[pos].size(); ++j) {
                        std::string new_full = current_full + char_pinyins[pos][j];
                        std::string new_initials = current_initials;
                        if (j < char_initials[pos].size()) {
                            new_initials += char_initials[pos][j];
                        }
                        generate_combinations(pos + 1, new_full, new_initials);
                    }
                };
            
            generate_combinations(0, "", "");
            
            // 检查所有拼音组合是否匹配 (Check if any pinyin combination matches)
            for (const auto& pinyin_combo : all_pinyin_combinations) {
                if (pinyin_combo.find(search_lower) != std::string::npos) {
                    matched = true;
                    break;
                }
            }
            
            // 如果完整拼音没匹配，检查首字母组合 (If full pinyin doesn't match, check initial combinations)
            if (!matched) {
                for (const auto& initial_combo : all_initial_combinations) {
                    if (initial_combo.find(search_lower) != std::string::npos) {
                        matched = true;
                        break;
                    }
                }
            }
        }
        
        // 如果匹配成功，添加字符串和索引到结果中 (If matched, add string and index to results)
        if (matched) {
            search_results.emplace_back(mapped_name, idx);  // 直接存储字符串结果和整数索引 (Directly store string result and integer index)
            // 限制最多显示6个结果 (Limit to maximum 6 results)
            if (search_results.size() >= 6) {
                break;
            }
        }
    }
    
    // 更新搜索状态 (Update search status)
    search_has_results = !search_results.empty();
}

// 清理AddGame界面的所有资源和状态 (Clear all AddGame interface resources and states)
void App::clearaddgamelist() {
    // 彻底清理AddGame界面的所有资源 (Thoroughly clean up all AddGame interface resources)
    {
        std::lock_guard<std::mutex> lock(this->entries_AddGame_mutex);
        for (auto& entry : this->entries_AddGame) {
            if (entry.own_image && entry.image > 0) {
                nvgDeleteImage(this->vg, entry.image);
                entry.image = 0;
                entry.own_image = false;
            }
            // 清理缓存的图标数据 (Clear cached icon data)
            entry.cached_icon_data.clear();
            entry.has_cached_icon = false;
        }
        this->entries_AddGame.clear(); // 清空列表 (Clear the list)
    }
    
    // 重置所有AddGame相关状态 (Reset all AddGame related states)
    addgame_scanned_count = 0;
    addgame_total_count = 0;
    addgame_scan_running = false;
    
    // 重置AddGame界面的索引变量 (Reset AddGame interface index variables)
    this->index_AddGame = 0;
    this->start_AddGame = 0;
    this->yoff_AddGame = 130.f;
    this->ypos_AddGame = 130.f;
    
    // 重置AddGame界面的防抖缓存，确保重新进入时能正确加载图标 (Reset AddGame interface debounce cache to ensure proper icon loading on re-entry)
    this->last_addgame_loaded_range = {SIZE_MAX, SIZE_MAX};
    this->last_addgame_load_time = {};
}

/**
 * @brief App类的析构函数
 * 负责清理应用程序使用的所有资源，确保没有内存泄漏
 */
App::~App() {
    // 清理模组名称缓存 (Cleanup mod name cache)
    ClearModNameCache();
    
    // 清理游戏名称缓存 (Cleanup game name cache)
    ClearGameNameCache();
    
    // 请求停止音频异步初始化 (Request to stop audio async initialization)
    this->audio_manager.RequestStop();
    
    // 清理音效管理器 (Cleanup audio manager)
    this->audio_manager.Cleanup();

    // 强制关闭
    if (copy_task.valid()) {
        copy_task.request_stop();  // 尽量通知停止
        copy_task = util::AsyncFurture<bool>{};  // 直接重置，丢弃原future
    }
    
    // 检查异步线程是否有效，如果有效则停止并等待其完成
    if (this->async_thread.valid()) {
        this->async_thread.request_stop(); // 请求线程停止
        this->async_thread.get(); // 等待线程完成
    }


    


    // 遍历所有应用条目，释放自行管理的图像资源
    for (auto&p : this->entries) {
        if (p.own_image) { // 仅释放由应用自己创建的图像
            nvgDeleteImage(this->vg, p.image); // 删除nanovg图像
        }
    }
    
    // 遍历所有添加游戏条目，释放自行管理的图像资源 (Cleanup AddGame entries image resources)
    for (auto&p : this->entries_AddGame) {
        if (p.own_image) { // 仅释放由应用自己创建的图像
            nvgDeleteImage(this->vg, p.image); // 删除nanovg图像
        }
    }

    // 释放默认图标图像资源 (Release default icon image resources)
    nvgDeleteImage(this->vg, default_icon_image);
    nvgDeleteImage(this->vg, like_image);
    
    // 只有在MOD图标已加载时才释放 (Only release MOD icons if they were loaded)
    if (mod_icons_loaded) {
        nvgDeleteImage(this->vg, Cheat_code_MOD_image);
        nvgDeleteImage(this->vg, Cosmetic_MOD_image);
        nvgDeleteImage(this->vg, FPS_MOD_image);
        nvgDeleteImage(this->vg, HD_MOD_image);
        nvgDeleteImage(this->vg, More_PLAY_MOD_image);
        nvgDeleteImage(this->vg, NONE_MOD_image);
    }

    // GPU命令优化：等待所有命令缓冲区完成执行
    // GPU command optimization: Wait for all command buffers to complete execution
    for (unsigned i = 0; i < NumCommandBuffers; ++i) {
        this->waitForCommandCompletion(i);
    }
    
    // 销毁帧缓冲区相关资源
    this->destroyFramebufferResources();

    // 删除nanovg上下文
    nvgDeleteDk(this->vg);

    // 刷新缓存文件
    nxtcFlushCacheFile();

    // 退出libnxtc库
    nxtcExit();

    // 重置渲染器指针，释放相关资源
    this->renderer.reset();

    
}

void App::createFramebufferResources() {
    // Create layout for the depth buffer
    dk::ImageLayout layout_depthbuffer;
    dk::ImageLayoutMaker{device}
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_HwCompression)
        .setFormat(DkImageFormat_S8)
        .setDimensions(1280, 720)
        .initialize(layout_depthbuffer);

    // Create the depth buffer
    this->depthBuffer_mem = this->pool_images->allocate(layout_depthbuffer.getSize(), layout_depthbuffer.getAlignment());
    this->depthBuffer.initialize(layout_depthbuffer, this->depthBuffer_mem.getMemBlock(), this->depthBuffer_mem.getOffset());

    // Create layout for the framebuffers
    dk::ImageLayout layout_framebuffer;
    dk::ImageLayoutMaker{device}
        .setFlags(DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression)
        .setFormat(DkImageFormat_RGBA8_Unorm)
        .setDimensions(1280, 720)
        .initialize(layout_framebuffer);

    // Create the framebuffers
    std::array<DkImage const*, NumFramebuffers> fb_array;
    const uint64_t fb_size  = layout_framebuffer.getSize();
    const uint32_t fb_align = layout_framebuffer.getAlignment();
    for (unsigned i = 0; i < fb_array.size(); i++) {
        // Allocate a framebuffer
        this->framebuffers_mem[i] = pool_images->allocate(fb_size, fb_align);
        this->framebuffers[i].initialize(layout_framebuffer, framebuffers_mem[i].getMemBlock(), framebuffers_mem[i].getOffset());

        // Generate a command list that binds it
        dk::ImageView colorTarget{ framebuffers[i] }, depthTarget{ depthBuffer };
        this->cmdbuf.bindRenderTargets(&colorTarget, &depthTarget);
        this->framebuffer_cmdlists[i] = cmdbuf.finishList();

        // Fill in the array for use later by the swapchain creation code
        fb_array[i] = &framebuffers[i];
    }

    // Create the swapchain using the framebuffers
    // 启用垂直同步以避免画面撕裂
    NWindow* nwin = nwindowGetDefault();
    nwindowSetSwapInterval(nwin, 1); // 设置交换间隔为1，启用垂直同步
    this->swapchain = dk::SwapchainMaker{device, nwin, fb_array}.create();

    // Generate the main rendering cmdlist
    this->recordStaticCommands();
}

void App::destroyFramebufferResources() {
    // Return early if we have nothing to destroy
    if (!this->swapchain) {
        return;
    }

    this->queue.waitIdle();
    this->cmdbuf.clear();
    swapchain.destroy();

    // Destroy the framebuffers
    for (unsigned i = 0; i < NumFramebuffers; i++) {
        framebuffers_mem[i].destroy();
    }

    // Destroy the depth buffer
    this->depthBuffer_mem.destroy();
}

void App::recordStaticCommands() {
    // Initialize state structs with deko3d defaults
    dk::RasterizerState rasterizerState;
    dk::ColorState colorState;
    dk::ColorWriteState colorWriteState;
    dk::BlendState blendState;

    // Configure the viewport and scissor
    this->cmdbuf.setViewports(0, { { 0.0f, 0.0f, 1280, 720, 0.0f, 1.0f } });
    this->cmdbuf.setScissors(0, { { 0, 0, 1280, 720 } });

    // Clear the color and depth buffers
    this->cmdbuf.clearColor(0, DkColorMask_RGBA, 0.2f, 0.3f, 0.3f, 1.0f);
    this->cmdbuf.clearDepthStencil(true, 1.0f, 0xFF, 0);

    // Bind required state
    this->cmdbuf.bindRasterizerState(rasterizerState);
    this->cmdbuf.bindColorState(colorState);
    this->cmdbuf.bindColorWriteState(colorWriteState);

    this->render_cmdlist = this->cmdbuf.finishList();
}

// GPU命令优化：准备下一个命令缓冲区
// GPU command optimization: Prepare next command buffer
void App::prepareNextCommandBuffer() {
    // 等待当前命令缓冲区完成（如果已提交）
    // Wait for current command buffer completion (if submitted)
    if (this->command_submitted[this->current_cmdbuf_index]) {
        this->waitForCommandCompletion(this->current_cmdbuf_index);
        this->command_submitted[this->current_cmdbuf_index] = false;
    }
    
    // 切换到下一个命令缓冲区
    // Switch to next command buffer
    this->current_cmdbuf_index = (this->current_cmdbuf_index + 1) % NumCommandBuffers;
    
    // 重新初始化当前命令缓冲区的命令列表
    // Re-initialize command list for current command buffer
    this->dynamic_cmdlists[this->current_cmdbuf_index] = this->dynamic_cmdbufs[this->current_cmdbuf_index].finishList();
}

// GPU命令优化：提交当前命令缓冲区
// GPU command optimization: Submit current command buffer
void App::submitCurrentCommandBuffer() {
    // 设置同步点
    // Set synchronization point
    this->dynamic_cmdbufs[this->current_cmdbuf_index].signalFence(this->command_fences[this->current_cmdbuf_index]);
    
    // 提交命令列表
    // Submit command list
    this->queue.submitCommands(this->dynamic_cmdlists[this->current_cmdbuf_index]);
    this->command_submitted[this->current_cmdbuf_index] = true;
}

// GPU命令优化：等待指定命令缓冲区完成
// GPU command optimization: Wait for specified command buffer completion
void App::waitForCommandCompletion(unsigned buffer_index) {
    if (buffer_index < NumCommandBuffers && this->command_submitted[buffer_index]) {
        // 等待fence信号
        // Wait for fence signal
        this->command_fences[buffer_index].wait();
    }
}



// ================= 字符串处理辅助函数 ========================


// 直接获取当前选中游戏的modjson路径
std::string App::GetModJsonPath(){

    std::string mod_json_path;

    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        mod_json_path = this->entries[this->index].FILE_PATH + "/mod_name.json";
    }

    return mod_json_path;

}


std::string App::GetGameDirName(){

    std::string FILE_PATH;

    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        FILE_PATH = this->entries[this->index].FILE_PATH;
        
    }

    // 提取游戏名字: /mods2/游戏名字/ID -> 游戏名字
    std::string game_dir_name = FILE_PATH.substr(7); // 跳过"/mods2/"，得到"游戏名字/ID"
    size_t pos = game_dir_name.find('/'); // 在跳过"/mods2/"后的字符串中查找'/'
    game_dir_name = game_dir_name.substr(0, pos); // 取第一个'/'之前的部分，即游戏名字

    return game_dir_name;

}

std::string App::GetModDirName(){

    std::string MOD_PATH = this->mod_info[this->mod_index].MOD_PATH;

    // 提取模组文件夹名字: /mods2/游戏名字/ID/模组名 -> 模组名
    std::string mod_dir_name = MOD_PATH.substr(MOD_PATH.find_last_of("/\\") + 1);

    return mod_dir_name;

}


// ================= 字符串处理辅助函数结束 ========================














void App::changemodname(){

    std::string MOD_NAME2 = this->mod_info[this->mod_index].MOD_NAME2;

    std::string mod_name = KeyboardHelper::showKeyboard(NAME_INPUT_TEXT.c_str(), MOD_NAME2.c_str());

    if (mod_name.empty()) {
        return;
    }

    std::string json_path = GetModJsonPath();

    std::string root_key = GetModDirName();

    if (!JsonManager::UpdateNestedJsonKeyValue(json_path, root_key, "display_name", mod_name)) return;
  
    this->mod_info[this->mod_index].MOD_NAME2 = mod_name;
    this->audio_manager.PlayConfirmSound(1.0);
 
}

void App::changemoddescription(){

    // 获取当前选中模组的描述内容
    std::string mod_description_text;
    mod_description_text = GetModDescription(mod_info[this->mod_index].MOD_NAME, mod_info[this->mod_index].MOD_TYPE);

    // 显示键盘输入框
    std::string description = KeyboardHelper::showKeyboard(MOD_DESCRIPTION_TEXT.c_str(), mod_description_text.c_str(),126,true);

    if (description.empty()) {
        return;
    }

    // 获取当前选中模组的json路径
    std::string json_path = GetModJsonPath();
    // 获取当前选中模组的路径名
    std::string root_key = GetModDirName();

    if (!JsonManager::UpdateNestedJsonKeyValue(json_path, root_key, "description", description)) return;

    std::string FILE_PATH;

    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        FILE_PATH = this->entries[this->index].FILE_PATH;
    }

    // 刷新mod列表数据
    LoadModNameMapping(FILE_PATH);
    
}


void App::tofavorite(){

    // 游戏的映射名
    std::string file_name2;

    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        file_name2 = this->entries[this->index].FILE_NAME2;
        
    }

    // 获取路径上的游戏名字，作为key
    std::string game_dir_name = GetGameDirName();

    // 从非favorites中删除
    if (!JsonManager::RemoveRootJsonKeyValue("/mods2/game_name.json", game_dir_name)) return;
    // 添加到favorites$180中
    if (!JsonManager::UpdateNestedJsonKeyValue("/mods2/game_name.json", "favorites$180", game_dir_name, file_name2)) return;

    // 修改结构体的版本值和文件路径，从而刷新UI
    {
        std::scoped_lock lock{entries_mutex};
        this->entries[this->index].is_favorite = true;
    }

    Sort();

}

void App::notfavorite(){

    // 游戏的映射名
    std::string file_name2;

    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        file_name2 = this->entries[this->index].FILE_NAME2;
        
    }

    // 获取路径上的游戏名字，作为key
    std::string game_dir_name = GetGameDirName();

    // 从favorites$180中删除
    if (!JsonManager::RemoveNestedJsonKeyValue("/mods2/game_name.json", "favorites$180", game_dir_name)) return;
    // 从添加到非favorites中
    if (!JsonManager::UpdateRootJsonKeyValue("/mods2/game_name.json", game_dir_name, file_name2)) return;

    // 修改结构体的版本值和文件路径，从而刷新UI
    {
        std::scoped_lock lock{entries_mutex};
        this->entries[this->index].is_favorite = false;
    }

    Sort();


}

void App::RemoveGame(){

    std::string FILE_PATH;

    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        FILE_PATH = this->entries[this->index].FILE_PATH;

    }

    // 为Removemodeformodlist创建错误回调函数 (Create error callback function)
    auto error_callback = [this](const std::string& error_message) {
        this->audio_manager.PlayCancelSound(1.0);
        // 显示错误对话框 (Show error dialog)
        newShowDialogConfirm(error_message);
    };

    // 调用具体工作函数
    if(!this->mod_manager.Removegameandallmods(FILE_PATH,error_callback)){
        return;
    }

    // 删除当前选中的游戏的entries，刷新界面
    {
        std::scoped_lock lock{entries_mutex};
        if (this->index < this->entries.size()) {
            this->entries.erase(this->entries.begin() + this->index);
            // 如果删除的是最后一个元素，调整索引
            if (this->index >= this->entries.size() && this->index > 0) {
                this->index--;
            }
        }
    }


}


void App::RemoveMod(){

    // 检查当前mod状态
    if (this->mod_info[this->mod_index].MOD_STATE) {
        this->audio_manager.PlayCancelSound(1.0);
        newShowDialogConfirm(CANNOT_REMOVE_INSTALLED_MOD);
        return;
    }

    // 要检查是否是最后一个mod，如果是最后一个mod就提示删除整个游戏
    if (this->mod_info.size() == 1) {
        this->audio_manager.PlayCancelSound(1.0);
        // 创建回调函数来处理用户确认后的操作
        auto callback = [this](bool confirmed) {
            if (confirmed) {
                this->menu_mode = MenuMode::LIST;
                RemoveGame();
            } 
        };
        newShowDialogConfirm(REMOVE_LAST_MOD_CONFIRM,callback);
        return;
    }


    // 要删除的mod目录路径和映射名
    std::string mod_path = this->mod_info[this->mod_index].MOD_PATH;
    std::string mod_name2 = this->mod_info[this->mod_index].MOD_NAME2;
    
    // 获取当前选中游戏的modjson路径
    std::string json_path = GetModJsonPath();

    // 为Removemodeformodlist创建错误回调函数 (Create error callback function)
    auto error_callback = [this](const std::string& error_message) {
        this->audio_manager.PlayCancelSound(1.0);
        // 显示错误对话框 (Show error dialog)
        newShowDialogConfirm(error_message);
    };

    // 如果失败就直接停止，负责具体操作
    if(!this->mod_manager.Removemodeformodlist(mod_path, error_callback)) return;

    // 删除当前选中的mod的mod_info刷新界面，同时调整相关索引，避免列表乱了
    if (this->mod_index < this->mod_info.size()) {
        this->mod_info.erase(this->mod_info.begin() + this->mod_index);
        
        // 调整mod_index，防止越界 (Adjust mod_index to prevent out of bounds)
        if (this->mod_info.empty()) {
            this->mod_index = 0;
        } else if (this->mod_index >= this->mod_info.size()) {
            this->mod_index = this->mod_info.size() - 1;
        }
        
        // 重新调整显示相关变量 (Readjust display related variables)
        if (this->mod_info.size() > 4) {
            std::size_t max_start = this->mod_info.size() - 4;
            if (this->mod_start > max_start) {
                this->mod_start = max_start;
            }
        } else {
            this->mod_start = 0;
        }
        
        // 更新显示位置 (Update display position)
        this->mod_ypos = 130.f + (this->mod_index - this->mod_start) * this->BOX_HEIGHT;
    }

}


// 修改mod版本实现函数
void App::changemodversion(){

    std::string MOD_VERSION;
    std::string FILE_NAME;
    std::string appid;      

    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        MOD_VERSION = this->entries[this->index].MOD_VERSION;
        FILE_NAME =  this->entries[this->index].FILE_NAME;
        // 将u64转换为16位十六进制字符串格式 (Convert u64 to 16-digit hex string format)
        char app_id_buffer[17]; // 16位十六进制字符串 + 结束符 (16-digit hex string + null terminator)
        snprintf(app_id_buffer, sizeof(app_id_buffer), "%016lX", this->entries[this->index].id);
        appid = app_id_buffer;
    }

    std::string version = KeyboardHelper::showNumberKeyboard(MOD_VERSION_INPUT_TEXT.c_str(), MOD_VERSION.c_str());
    
    
    if (version.empty()) {
        return;
    }

    std::string old_file_path;

    if (MOD_VERSION == NONE_TYPE_TEXT) {
        old_file_path = "/mods2/" + FILE_NAME;
    }
    else old_file_path = "/mods2/" + FILE_NAME + "[" + MOD_VERSION + "]";
    
    std::string new_file_path = "/mods2/" + FILE_NAME + "[" + version + "]";
    

    // 检查新路径是否已存在 (Check if new path already exists)
    struct stat path_stat;
    if (stat(new_file_path.c_str(), &path_stat) == 0) {
        newShowDialogConfirm(VERSION_GAME_EXISTS);
        this->audio_manager.PlayCancelSound(1.0);
        return;
    }
    
    std::string old_key = GetGameDirName();
    std::string new_key = FILE_NAME + "[" + version + "]";
    
    // 尝试重命名游戏目录名字 如XXXX[X.X] ===> XXXX[version]
    if (rename(old_file_path.c_str(), new_file_path.c_str()) == 0) {

        JsonManager::RenameOrCreateJsonRootKey("/mods2/game_name.json", old_key, new_key);

        // 修改结构体的版本值和文件路径，从而刷新UI
        {
            std::scoped_lock lock{entries_mutex};
            this->entries[this->index].MOD_VERSION = version;
            this->entries[this->index].FILE_PATH = new_file_path + "/" + appid;
        }
        this->audio_manager.PlayConfirmSound(1.0);

    }

}

// 自定义名称实现函数
void App::changegamename() {
    
    std::string FILE_NAME2;
    std::string FILE_PATH;
    bool is_favorite;

    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        FILE_NAME2 = this->entries[this->index].FILE_NAME2;
        FILE_PATH = this->entries[this->index].FILE_PATH;
        is_favorite = this->entries[this->index].is_favorite;

    }

    std::string name = KeyboardHelper::showKeyboard(NAME_INPUT_TEXT.c_str(), FILE_NAME2.c_str());
    
    if (name.empty()) {
        return;
    }

    // 从FILE_PATH中提取第二个目录部分作为json_key
    std::string json_key = GetGameDirName();

    // 如果这个游戏是标记喜欢的，就用去修改嵌套的json，否则正常修改。
    if (is_favorite) {
        if (!JsonManager::UpdateNestedJsonKeyValue("/mods2/game_name.json", "favorites$180",json_key, name)) return;
    }
    else {
        // 添加到非favorites中
        if (!JsonManager::UpdateRootJsonKeyValue("/mods2/game_name.json", json_key, name)) return;
    }
    
    // 写入操作需要锁保护
    {
        std::scoped_lock lock{entries_mutex};
        this->entries[this->index].FILE_NAME2 = name;
    }
    this->audio_manager.PlayConfirmSound(1.0);

}






// 修改模组类型界面右侧菜单处理中心 (MOD list interface right menu processing center)
void App::Dialog_MODLIST_TYPE_Menu(const std::string& type) {

    

    std::string mod_type;

    if (type == LIST_DIALOG_FPSPATCH) {
        mod_type = "[F]";
    }
    else if (type == LIST_DIALOG_HDPATCH) {
        mod_type = "[G]";
    }
    else if (type == LIST_DIALOG_COSMETICPATCH) {
        mod_type = "[B]";
    }
    else if (type == LIST_DIALOG_PLAYPATCH) {
        mod_type = "[P]";
    }
    else if (type == LIST_DIALOG_CHEATPATCH) {
        mod_type = "[C]";
    }
    
    
    std::string FILE_PATH;

    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        FILE_PATH = this->entries[this->index].FILE_PATH;
    }

    std::string MOD_NAME = this->mod_info[this->mod_index].MOD_NAME;

    bool MOD_STATE = this->mod_info[this->mod_index].MOD_STATE;
    
    std::string mod_type2 = mod_type;
    if (MOD_STATE) {
        mod_type2 = mod_type + "$";
    }

    std::string old_mod_path = this->mod_info[this->mod_index].MOD_PATH;

    std::string new_mod_path = FILE_PATH + "/" + MOD_NAME + mod_type2;

    std::string path = old_mod_path + "\n" + new_mod_path;

    if (rename(old_mod_path.c_str(), new_mod_path.c_str()) != 0) return;

    std::string old_root_key = GetModDirName();

    std::string new_root_key = MOD_NAME + mod_type;

    std::string json_path = GetModJsonPath();

    if (!JsonManager::RenameOrCreateJsonRootKey(json_path, old_root_key, new_root_key)) return;

    this->mod_info[this->mod_index].MOD_PATH = new_mod_path;
    this->mod_info[this->mod_index].MOD_TYPE = mod_type;

    LoadModNameMapping(FILE_PATH);
    
}

// 追加模组界面右侧菜单处理中心 (MOD list interface right menu processing center)
void App::Dialog_APPENDMOD_Menu(const std::vector<std::string>& selected_items) {
    
    if (selected_items.empty())  return;


    std::string FILE_PATH;

    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        FILE_PATH = this->entries[this->index].FILE_PATH;
    }

    // 立即显示添加进度对话框 (Immediately show add progress dialog)
    newShowDialogCopyProgress(ADDING_MOD_TEXT, PRESS_B_STOP);
    
    // 初始化进度信息 (Initialize progress info)
    {
        std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
        this->copy_progress = {};
    }
    
    // 启动异步添加任务 (Start async add task)
    add_task = util::async([this, selected_items, FILE_PATH](std::stop_token stop_token) -> bool {
        // 开始计时 (Start timing)
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 更新总文件数 (Update total files)
        {
            std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
            this->copy_progress.total_files = selected_items.size();
        }
        
        // 遍历所有选中的mod进行添加 (Iterate through all selected mods for addition)
        size_t current_index = 0;
        for (const std::string& selected_mod : selected_items) {
            // 检查是否需要停止 (Check if stop is requested)
            if (stop_token.stop_requested()) {
                
                return false;
            }
            
            // 更新当前处理的mod名称 (Update current processing mod name)
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                this->copy_progress.current_file = selected_mod;
                this->copy_progress.files_copied = current_index;
                this->copy_progress.progress_percentage = (float)current_index / selected_items.size() * 100.0f;
            }
            this->newUpdateCopyProgress(this->copy_progress);
            
            std::string old_mod_path = "/mods2/0000-add-mod-0000/" + selected_mod;
            // 去掉.ZIP或.zip后缀获取模组目录名 (Remove .ZIP or .zip suffix to get mod directory name)
            std::string new_dir_name = selected_mod.substr(0, selected_mod.size() - 4);
            std::string new_dir_path = FILE_PATH + "/" + new_dir_name;
            std::string new_mod_path = new_dir_path + "/" + selected_mod;
            
            // 创建新目录 (Create new directory)
            if (mkdir(new_dir_path.c_str(), 0777) != 0) {
                // 如果目录已存在则继续，其他错误则跳过当前mod (Continue if directory exists, skip current mod for other errors)
                if (errno != EEXIST) {
                    continue;
                }
            }
            
            // 循环检查目标文件是否存在，如果存在则重新生成路径 (Loop to check if target file exists, regenerate path if it does)
            for (int attempt = 1; attempt <= 10; attempt++) {
                struct stat file_stat;
                if (stat(new_mod_path.c_str(), &file_stat) != 0) {
                    // 文件不存在，可以使用这个路径 (File doesn't exist, can use this path)
                    break;
                }
                // 文件存在，生成新的路径 (File exists, generate new path)
                new_mod_path = new_dir_path + "/" + std::to_string(attempt) + "-" + selected_mod;
            } 

            // 移动文件 (Move file)
            if (rename(old_mod_path.c_str(), new_mod_path.c_str()) != 0) {
                // 如果目标文件已存在则继续，其他错误则跳过当前mod (Continue if target file exists, skip current mod for other errors)
                continue;
            }

            // 向std::vector<MODINFO> mod_info中追加一个结构体，便于modlist界面立马刷新
            MODINFO mod_info2;
            mod_info2.MOD_NAME = new_dir_name;
            mod_info2.MOD_NAME2 = new_dir_name;
            mod_info2.MOD_PATH = new_dir_path;
            mod_info2.MOD_TYPE = NONE_TYPE_TEXT;
            mod_info2.MOD_STATE = false;
            this->mod_info.push_back(mod_info2);
            
            // 增加计数器 (Increment counter)
            current_index++;
            
        }
        
        
        // 计算耗时 (Calculate duration)
        auto end_time = std::chrono::high_resolution_clock::now();
        this->operation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // 更新最终进度 (Update final progress)
        CopyProgressInfo final_progress;
        {
            std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
            final_progress = this->copy_progress;
        }
        final_progress.is_completed = true;
        final_progress.files_copied = selected_items.size();
        final_progress.progress_percentage = 100.0f;
        this->newUpdateCopyProgress(final_progress);
        
        return true;
    });

    
}




// 添加游戏界面右侧菜单处理中心 (MOD list interface right menu processing center)
void App::Dialog_ADDGAME_Menu(const std::vector<std::string>& selected_items) {

    if (selected_items.empty())  return;


    auto index_copy = this->index_AddGame;

    // 预先复制游戏信息
    u64 application_id;
    std::string name;
    std::string display_version;
    std::vector<unsigned char> cached_icon_data;
    bool has_cached_icon;

    {
        std::scoped_lock lock{entries_AddGame_mutex}; // 加锁保护entries_AddGame的访问 (Lock to protect entries_AddGame access)
        
        application_id = this->entries_AddGame[index_copy].id;
        name = this->entries_AddGame[index_copy].name;
        display_version = this->entries_AddGame[index_copy].display_version;
        cached_icon_data = this->entries_AddGame[index_copy].cached_icon_data;
        has_cached_icon = this->entries_AddGame[index_copy].has_cached_icon;

        
    }

    

    // // 立即显示添加游戏进度对话框 (Immediately show add game progress dialog)
    newShowDialogCopyProgress(ADDING_MOD_TEXT, PRESS_B_STOP);
    
    // 初始化进度信息 (Initialize progress info)
    {
        std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
        this->copy_progress = {};
    }

   

    // 启动异步添加游戏任务 (Start async add game task)
    add_task = util::async([this, selected_items, application_id, name, display_version, cached_icon_data, has_cached_icon](std::stop_token stop_token) -> bool {
        
        auto start_time = std::chrono::high_resolution_clock::now();

        // 在异步任务中获取应用英文名称 (Get app English name in async task)
        std::string app_english_name = TryGetAppEnglishName(application_id);

        // 在异步任务中创建目录结构 (Create directory structure in async task)
        std::string game_name_path = "/mods2/" + app_english_name;

        if (mkdir(game_name_path.c_str(), 0777) != 0) {
            // 如果目录已存在则继续，其他错误则返回失败 (Continue if directory exists, return failure for other errors)
            if (errno != EEXIST) {
                // 记录目录创建失败的错误信息 (Record directory creation failure error)
                {
                    std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                    this->copy_progress.has_error = true;
                    this->copy_progress.error_message = CREATE_GAME_DIR_ERROR + ": " + game_name_path + " error: " + std::to_string(errno);
                }
                return false;
            } else {

                // 当前游戏已添加，请勿重复添加。
                {
                    std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                    this->copy_progress.has_error = true;
                    this->copy_progress.error_message = READD_ERROR;
                }
                return false;

            }
        }

        char app_id_buffer[17]; // 16位十六进制字符串 + 结束符 (16-digit hex string + null terminator)
        snprintf(app_id_buffer, sizeof(app_id_buffer), "%016lX", application_id);
        std::string app_id = std::string(app_id_buffer);

        std::string game_name_id_path = "/mods2/" + app_english_name + "/" + app_id;

        if (mkdir(game_name_id_path.c_str(), 0777) != 0) {
            // 如果目录已存在则继续，其他错误则返回失败 (Continue if directory exists, return failure for other errors)
            if (errno != EEXIST) {
                // 记录游戏ID目录创建失败的错误信息 (Record game ID directory creation failure error)
                {
                    std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                    this->copy_progress.has_error = true;
                    this->copy_progress.error_message = CREATE_GAME_ID_DIR_ERROR + ": " + game_name_id_path + " error: " + std::to_string(errno);
                }
                return false;
            }
        }

        // 更新总文件数 (Update total files)
        {
            std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
            this->copy_progress.total_files = selected_items.size();
        }

        int successfully_added_mods = 0; // 统计成功添加的mod数量 (Count successfully added mods)
        
        // 遍历所有选中的mod进行添加 (Iterate through all selected mods for addition)
        size_t current_index = 0;
        for (const std::string& selected_mod : selected_items) {
            // 检查是否需要停止 (Check if stop is requested)
            if (stop_token.stop_requested()) {
                return false;
            }
            
            // 更新当前处理的mod名称 (Update current processing mod name)
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                this->copy_progress.current_file = selected_mod;
                this->copy_progress.files_copied = current_index;
                this->copy_progress.progress_percentage = (float)current_index / selected_items.size() * 100.0f;
            }
            this->newUpdateCopyProgress(this->copy_progress);
            
            std::string old_mod_path = "/mods2/0000-add-mod-0000/" + selected_mod;
            // 去掉.ZIP或.zip后缀获取模组目录名 (Remove .ZIP or .zip suffix to get mod directory name)
            std::string new_dir_name = selected_mod.substr(0, selected_mod.size() - 4);
            std::string new_dir_path = game_name_id_path + "/" + new_dir_name;
            std::string new_mod_path = new_dir_path + "/" + selected_mod;

            if (mkdir(new_dir_path.c_str(), 0777) != 0) {
                // 如果目录已存在则继续，其他错误则跳过当前mod (Continue if directory exists, skip current mod for other errors)
                if (errno != EEXIST) {
                    current_index++;
                    continue;
                }
            }

            // 循环检查目标文件是否存在，如果存在则重新生成路径 (Loop to check if target file exists, regenerate path if it does)
            for (int attempt = 1; attempt <= 10; attempt++) {
                struct stat file_stat;
                if (stat(new_mod_path.c_str(), &file_stat) != 0) {
                    // 文件不存在，可以使用这个路径 (File doesn't exist, can use this path)
                    break;
                }
                // 文件存在，生成新的路径 (File exists, generate new path)
                new_mod_path = new_dir_path + "/" + std::to_string(attempt) + "-" + selected_mod;
            } 
            
            // 移动文件 (Move file)
            if (rename(old_mod_path.c_str(), new_mod_path.c_str()) != 0) {
                // 如果目标文件已存在则继续，其他错误则跳过当前mod (Continue if target file exists, skip current mod for other errors)
                current_index++;
                continue;
            }
            
            successfully_added_mods++; // 成功添加一个mod (Successfully added one mod)
            current_index++;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        this->operation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        // 更新最终进度 (Update final progress)
        CopyProgressInfo final_progress;
        {
            std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
            final_progress = this->copy_progress;
        }
        final_progress.is_completed = true;
        final_progress.files_copied = selected_items.size();
        final_progress.progress_percentage = 100.0f;
        this->newUpdateCopyProgress(final_progress);

        AppEntry app_entry;
        app_entry.name = name;
        app_entry.display_version = display_version;
        app_entry.id = application_id;
        app_entry.image = this->default_icon_image; // 先设置为默认图标，等待异步加载 (Set to default icon first, wait for async loading)
        app_entry.own_image = false;
        app_entry.cached_icon_data = cached_icon_data;
        app_entry.has_cached_icon = has_cached_icon;
        app_entry.FILE_NAME = app_english_name;
        app_entry.FILE_NAME2 = name;
        app_entry.FILE_PATH = game_name_id_path;
        app_entry.MOD_VERSION = NONE_TYPE_TEXT;
        app_entry.MOD_TOTAL = std::to_string(successfully_added_mods);
        app_entry.unique_id = unique_id++;

        // 写入游戏名称到JSON文件 (Write game name to JSON file)
        JsonManager::UpdateRootJsonKeyValue("/mods2/game_name.json", app_english_name, name);

        // 加锁保护entries列表
        {
            std::scoped_lock lock{entries_mutex};
            // 将应用条目添加到列表最前面
            this->entries.emplace(this->entries.begin(), std::move(app_entry));
        }

        // 重置主界面光标和页面位置，让用户返回桌面时能看到新添加的游戏 (Reset main interface cursor and page position so user can see newly added game when returning to desktop)
        this->index = 0;        // 重置光标到第一个位置 (Reset cursor to first position)
        this->start = 0;        // 重置显示起始索引 (Reset display start index)
        this->yoff = 130.f;     // 重置Y坐标偏移量 (Reset Y coordinate offset)
        this->ypos = 130.f;     // 重置Y坐标位置 (Reset Y coordinate position)
        
        // 直接为新添加的游戏加载图标，避免重新加载所有图标 (Load icon for newly added game directly, avoid reloading all icons)
        if (app_entry.display_version != NONE_GAME_TEXT && app_entry.has_cached_icon) {
            ResourceLoadTask icon_task;
            icon_task.unique_id = app_entry.unique_id;
            icon_task.priority = 0; // 新添加的游戏给予最高优先级 (Give highest priority to newly added games)
            icon_task.submit_time = std::chrono::steady_clock::now();
            icon_task.task_type = ResourceTaskType::ICON;
            
            icon_task.load_callback = [this, unique_id = app_entry.unique_id]() {
                std::vector<unsigned char> icon_data;
                bool has_icon_data = false;
                
                // 获取缓存的图标数据 (Get cached icon data)
                {
                    std::scoped_lock lock{entries_mutex};
                    auto it = std::find_if(entries.begin(), entries.end(),
                        [unique_id](const AppEntry& entry) {
                            return entry.unique_id == unique_id && entry.has_cached_icon;
                        });
                    
                    if (it != entries.end()) {
                        icon_data = it->cached_icon_data;
                        has_icon_data = true;
                    }
                }
                
                if (!has_icon_data) {
                    return;
                }
                
                // 验证并创建图像 (Validate and create image)
                if (!icon_data.empty() && IsValidJpegData(icon_data)) {
                    int image_id = nvgCreateImageMem(this->vg, 0, icon_data.data(), icon_data.size());
                    if (image_id > 0) {
                        std::scoped_lock lock{entries_mutex};
                        auto it = std::find_if(entries.begin(), entries.end(),
                            [unique_id](const AppEntry& entry) {
                                return entry.unique_id == unique_id;
                            });
                        
                        if (it != entries.end()) {
                            if (it->own_image && it->image != this->default_icon_image) {
                                nvgDeleteImage(this->vg, it->image);
                            }
                            it->image = image_id;
                            it->own_image = true;
                        }
                    }
                }
            };
            
            this->resource_manager.submitLoadTask(icon_task);
        }

        

        
        
        return true;
    });

}



// 更新教程界面 (Update instruction interface)
void App::UpdateInstruction() {
    // 更新边界闪烁动画 (Update boundary flash animation)
    if (this->boundary_flash_animation > 0) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - this->boundary_animation_start_time).count();
        
        if (elapsed_ms >= BOUNDARY_ANIMATION_DURATION_MS) {
            // 边界动画完成 (Boundary animation complete)
            this->boundary_flash_animation = 0;
        }
    }
    
    // 更新三角形透明度动画 (Update triangle opacity animation)
    if (this->triangle_alpha_animation > 0) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - this->triangle_animation_start_time).count();
        
        if (this->triangle_alpha_animation == 1) { // 变亮阶段 (Brighten phase)
            if (elapsed_ms >= TRIANGLE_ANIMATION_DURATION_MS) {
                // 变亮完成，开始变暗 (Brighten complete, start darken)
                this->triangle_alpha_animation = 2;
                this->triangle_animation_start_time = current_time;
            }
        } else if (this->triangle_alpha_animation == 2) { // 变暗阶段 (Darken phase)
            if (elapsed_ms >= TRIANGLE_ANIMATION_DURATION_MS) {
                // 动画完成 (Animation complete)
                this->triangle_alpha_animation = 0;
            }
        }
    }
    
    // B键返回到主菜单 (B key to return to main menu)
    if (this->controller.B) {
        this->audio_manager.PlayKeySound(); 
        this->menu_mode = MenuMode::LIST; // 返回到应用列表界面 (Return to application list interface)
    }
    
    // 左键：上一张图片 (LEFT key: previous image)
    if (this->controller.LEFT) {
        if (this->instruction_image_index > 0) {
            // 正常切换到上一张图片 (Normal switch to previous image)
            this->audio_manager.PlayKeySound();
            this->instruction_image_index--;
            
            // 触发左侧三角形透明度动画 (Trigger left triangle opacity animation)
            this->triangle_alpha_animation = 1; // 开始变亮动画 (Start brighten animation)
            this->triangle_animation_is_left = true; // 设置为左侧三角形动画 (Set as left triangle animation)
            this->triangle_animation_start_time = std::chrono::steady_clock::now();
        } else {
            // 已经是第一张图片，触发左边界效果 (Already at first image, trigger left boundary effect)
            this->audio_manager.PlayLimitSound(); // 播放边界音效 (Play boundary sound)
            
            // 触发左边界闪烁动画 (Trigger left boundary flash animation)
            this->boundary_flash_animation = 1;
            this->boundary_animation_is_left = true; // 设置为左边界动画 (Set as left boundary animation)
            this->boundary_animation_start_time = std::chrono::steady_clock::now();
        }
    }
    
    // 右键：下一张图片 (RIGHT key: next image)
    constexpr int TOTAL_IMAGES = 8; // 总共8张教程图片 (Total 6 instruction images)
    if (this->controller.RIGHT) {
        if (this->instruction_image_index < TOTAL_IMAGES - 1) {
            // 正常切换到下一张图片 (Normal switch to next image)
            this->audio_manager.PlayKeySound();
            this->instruction_image_index++;
            
            // 触发右侧三角形透明度动画 (Trigger right triangle opacity animation)
            this->triangle_alpha_animation = 1; // 开始变亮动画 (Start brighten animation)
            this->triangle_animation_is_left = false; // 设置为右侧三角形动画 (Set as right triangle animation)
            this->triangle_animation_start_time = std::chrono::steady_clock::now();
        } else {
            // 已经是最后一张图片，触发右边界效果 (Already at last image, trigger right boundary effect)
            this->audio_manager.PlayLimitSound(); // 播放边界音效 (Play boundary sound)
            
            // 触发右边界闪烁动画 (Trigger right boundary flash animation)
            this->boundary_flash_animation = 1;
            this->boundary_animation_is_left = false; // 设置为右边界动画 (Set as right boundary animation)
            this->boundary_animation_start_time = std::chrono::steady_clock::now();
        }
    }
}

// 绘制教程界面 (Draw instruction interface)
void App::DrawInstruction() {
    // 绘制标题 (Draw title)
    SOFTWARE_TITLE = SOFTWARE_TITLE_INSTRUCTION;

    // 教程图片路径数组 (Instruction image paths array)
    const char* image_paths[] = {
        "romfs:/Instruction/1.jpg",
        "romfs:/Instruction/2.jpg",
        "romfs:/Instruction/3.jpg",
        "romfs:/Instruction/4.jpg",
        "romfs:/Instruction/5.jpg",
        "romfs:/Instruction/6.jpg",
        "romfs:/Instruction/7.jpg",
        "romfs:/Instruction/8.jpg",
    };


    constexpr int TOTAL_IMAGES = 8; // 总图片数量 (Total number of images)
    
    // 使用局部静态变量管理图片资源 (Use local static variables to manage image resources)
    static int cached_image_id = -1;        // 当前缓存的图片ID (Currently cached image ID)
    static int cached_image_index = -1;     // 当前缓存的图片索引 (Currently cached image index)
    
    // 检查是否需要加载新图片 (Check if need to load new image)
    if (cached_image_index != this->instruction_image_index) {
        // 删除之前的图片资源 (Delete previous image resource)
        if (cached_image_id >= 0) {
            nvgDeleteImage(this->vg, cached_image_id);
            cached_image_id = -1;
        }
        
        // 加载新图片 (Load new image)
        cached_image_id = nvgCreateImage(this->vg, image_paths[this->instruction_image_index], NVG_IMAGE_NEAREST);
        cached_image_index = this->instruction_image_index;
    }
    
    // 绘制当前图片 (Draw current image)
    if (cached_image_id >= 0) {
        // 绘制图片 (Draw image)
        const auto image_paint = nvgImagePattern(this->vg, 0, 0, 1280.0f, 720.0f, 0.0f, cached_image_id, 1.0f);
        gfx::drawRect(this->vg, 0, 0,1280.0f, 720.0f, image_paint);
    }
    
    // 绘制导航三角形 (Draw navigation triangles)
    const float triangle_width = 20.0f;  // 三角形宽度 (Triangle width)
    const float triangle_height = 40.0f; // 三角形高度 (Triangle height)
    const float margin = 50.0f;         // 距离边界的边距 (Margin from edge)
    const float center_y = 720.0f / 2.0f; // 屏幕中心Y坐标 (Screen center Y coordinate)
    const float corner_radius = 5.0f;   // 圆角半径 (Corner radius)
    
    // 计算左侧三角形透明度，考虑动画效果 (Calculate left triangle opacity with animation effect)
    int left_alpha = (this->instruction_image_index == 0) ? 0 : 20;  // 第一页时左三角形透明 (Left triangle transparent on first page)
    
    // 计算右侧三角形透明度，考虑动画效果 (Calculate right triangle opacity with animation effect)
    int right_alpha = (this->instruction_image_index == TOTAL_IMAGES - 1) ? 0 : 20; // 最后一页时右三角形透明 (Right triangle transparent on last page)
    
    // 应用三角形透明度动画效果 (Apply triangle opacity animation effect)
    if (this->triangle_alpha_animation > 0) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - this->triangle_animation_start_time).count();
        
        // 根据动画方向决定修改哪个三角形的透明度 (Decide which triangle's opacity to modify based on animation direction)
        int* target_alpha = this->triangle_animation_is_left ? &left_alpha : &right_alpha;
        
        // 只有在目标三角形可见时才应用动画效果 (Only apply animation effect when target triangle is visible)
        if (*target_alpha > 0) {
            if (this->triangle_alpha_animation == 1) { // 变亮阶段 (Brighten phase)
                // 从20变到100 (From 20 to 100)
                float progress = static_cast<float>(elapsed_ms) / TRIANGLE_ANIMATION_DURATION_MS;
                progress = std::min(progress, 1.0f);
                *target_alpha = static_cast<int>(20 + (100 - 20) * progress);
            } else if (this->triangle_alpha_animation == 2) { // 变暗阶段 (Darken phase)
                // 从100变回20 (From 100 back to 20)
                float progress = static_cast<float>(elapsed_ms) / TRIANGLE_ANIMATION_DURATION_MS;
                progress = std::min(progress, 1.0f);
                *target_alpha = static_cast<int>(100 - (100 - 20) * progress);
            }
        }
    }
    
    // 设置线条样式 (Set line style)
    nvgLineJoin(this->vg, NVG_ROUND);    // 设置线条连接为圆角 (Set line join to round)
    nvgLineCap(this->vg, NVG_ROUND);     // 设置线条端点为圆角 (Set line cap to round)
    
    // 绘制左侧圆角三角形，箭头朝向左边界 (Draw left rounded triangle, arrow pointing to left edge)
    if (left_alpha > 0) { // 只有在不透明时才绘制 (Only draw when not transparent)
        nvgFillColor(this->vg, nvgRGBA(255, 255, 255, left_alpha)); // 根据页面状态设置透明度 (Set opacity based on page state)
        
        nvgBeginPath(this->vg);
        nvgMoveTo(this->vg, margin, center_y);                           // 左顶点 (Left vertex)
        nvgLineTo(this->vg, margin + triangle_width, center_y - triangle_height / 2); // 右上顶点 (Right top vertex)
        nvgLineTo(this->vg, margin + triangle_width, center_y + triangle_height / 2); // 右下顶点 (Right bottom vertex)
        nvgClosePath(this->vg);
        
        // 使用描边方式绘制圆角效果 (Use stroke to create rounded effect)
        nvgStrokeWidth(this->vg, corner_radius * 2);
        nvgStrokeColor(this->vg, nvgRGBA(255, 255, 255, left_alpha));
        nvgStroke(this->vg);
        nvgFill(this->vg);
    }
    
    // 绘制右侧圆角三角形，箭头朝向右边界 (Draw right rounded triangle, arrow pointing to right edge)
    if (right_alpha > 0) { // 只有在不透明时才绘制 (Only draw when not transparent)
        nvgFillColor(this->vg, nvgRGBA(255, 255, 255, right_alpha)); // 根据页面状态设置透明度 (Set opacity based on page state)
        
        nvgBeginPath(this->vg);
        nvgMoveTo(this->vg, 1280.0f - margin, center_y);                           // 右顶点 (Right vertex)
        nvgLineTo(this->vg, 1280.0f - margin - triangle_width, center_y - triangle_height / 2); // 左上顶点 (Left top vertex)
        nvgLineTo(this->vg, 1280.0f - margin - triangle_width, center_y + triangle_height / 2); // 左下顶点 (Left bottom vertex)
        nvgClosePath(this->vg);
        
        // 使用描边方式绘制圆角效果 (Use stroke to create rounded effect)
        nvgStrokeWidth(this->vg, corner_radius * 2);
        nvgStrokeColor(this->vg, nvgRGBA(255, 255, 255, right_alpha));
        nvgStroke(this->vg);
        nvgFill(this->vg);
    }
    
    // 绘制边界闪烁效果 (Draw boundary flash effect)
    if (this->boundary_flash_animation > 0) {
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - this->boundary_animation_start_time).count();
        
        // 计算闪烁透明度，从0到最大值再到0的渐变效果 (Calculate flash opacity with fade in/out effect)
        float progress = static_cast<float>(elapsed_ms) / BOUNDARY_ANIMATION_DURATION_MS;
        progress = std::min(progress, 1.0f);
        
        // 使用正弦函数创建平滑的闪烁效果 (Use sine function for smooth flash effect)
        float flash_intensity = std::sin(progress * 3.14159f); // 0到1再到0的平滑过渡 (Smooth transition from 0 to 1 to 0)
        int flash_alpha = static_cast<int>(flash_intensity * 100); // 最大透明度为80 (Maximum opacity is 80)
        
        if (flash_alpha > 0) {
            // 确定闪烁区域：左边界或右边界 (Determine flash area: left or right boundary)
            float flash_width = 5.0f; // 闪烁区域宽度 (Flash area width)
            float flash_x = this->boundary_animation_is_left ? 0.0f : (1280.0f - flash_width); // 左边界从0开始，右边界从右侧开始 (Left boundary starts from 0, right boundary starts from right side)
            
            // 绘制边界闪烁矩形 (Draw boundary flash rectangle)
            nvgBeginPath(this->vg);
            nvgRect(this->vg, flash_x, 0.0f, flash_width, 720.0f);
            nvgFillColor(this->vg, nvgRGBA(255, 255, 255, flash_alpha)); // 白色闪烁效果 (White flash effect)
            nvgFill(this->vg);
        }
    }

}



// 切换当前选中模组的安装状态并修改目录名称 (Toggle current selected mod install status and modify directory name)
void App::ChangeModName() {
    
    // 获取当前选中的模组
    MODINFO& current_mod = mod_info[mod_index];
    
    // 获取当前的目录路径
    std::string current_path = current_mod.MOD_PATH;
    std::string new_path;
    
    // 根据当前状态决定新的路径名
    if (current_mod.MOD_STATE) {
        // 当前已安装，改为未安装，移除末尾的$符号
        if (!current_path.empty() && current_path.back() == '$') {
            new_path = current_path.substr(0, current_path.length() - 1);
        } else {
            return; // 如果没有$符号，说明状态不一致，直接返回
        }
    } else {
        // 当前未安装，改为已安装，在末尾添加$符号
        if (!current_path.empty() && current_path.back() != '$') {
            new_path = current_path + "$";
        } else {
            return; // 如果已有$符号，说明状态不一致，直接返回
        }
    }
    
    // 尝试重命名目录
    if (rename(current_path.c_str(), new_path.c_str()) == 0) {
        // 重命名成功，更新模组信息
        current_mod.MOD_STATE = !current_mod.MOD_STATE;
        current_mod.MOD_PATH = new_path;
    } 
    // 如果重命名失败，不做任何更改
}


// 排序MOD列表函数 (Sort MOD list function)
void App::Sort_Mod() {
    std::sort(this->mod_info.begin(), this->mod_info.end(), [this](const MODINFO& a, const MODINFO& b) {
        // 首先按安装状态排序：已安装的在前面 (First sort by installation status: installed ones first)
        if (a.MOD_STATE != b.MOD_STATE) {
            return a.MOD_STATE > b.MOD_STATE; // true > false，已安装的排在前面 (true > false, installed ones first)
        }
        
        // 在相同安装状态下，按类型排序 (Within same installation status, sort by type)
        // 定义类型优先级：有具体类型的在前面，无类型(NONE_TYPE_TEXT)的在后面 (Define type priority: with specific type first, without type(NONE_TYPE_TEXT) last)
        bool a_has_type = (a.MOD_TYPE != NONE_TYPE_TEXT);
        bool b_has_type = (b.MOD_TYPE != NONE_TYPE_TEXT);
        
        if (a_has_type != b_has_type) {
            return a_has_type > b_has_type; // 有具体类型的排在前面 (With specific type first)
        }
        
        // 如果都有具体类型或都是无类型(NONE_TYPE_TEXT)，按类型和拼音排序 (If both have specific type or both are NONE_TYPE_TEXT, sort by type and pinyin)
        if (a_has_type && b_has_type) {
            // 都有具体类型，先按类型字符串排序，类型相同则按名称拼音排序 (Both have specific type, sort by type string first, then by name pinyin if same type)
            if (a.MOD_TYPE != b.MOD_TYPE) {
                return a.MOD_TYPE < b.MOD_TYPE; // 直接按类型字符串排序 (Sort directly by type string)
            }
            std::string pinyin_a = this->GetFirstCharPinyin(a.MOD_NAME2);
            std::string pinyin_b = this->GetFirstCharPinyin(b.MOD_NAME2);
            return pinyin_a < pinyin_b; // 类型相同，按名称拼音顺序 (Same type, sort by name pinyin)
        } else {
            // 都是无类型(NONE_TYPE_TEXT)，直接按名称拼音排序 (Both are NONE_TYPE_TEXT, sort by name pinyin directly)
            std::string pinyin_a = this->GetFirstCharPinyin(a.MOD_NAME2);
            std::string pinyin_b = this->GetFirstCharPinyin(b.MOD_NAME2);
            return pinyin_a < pinyin_b; // 按名称拼音顺序 (Sort by name pinyin)
        }
    });
}



void App::MODinstallORuninstall(){

    // 获取当前选中的模组名称和状态 (Get current selected mod name and state)
    std::string select_mod_name = this->mod_info[this->mod_index].MOD_NAME2;
    bool select_mod_state = this->mod_info[this->mod_index].MOD_STATE;
    bool should_uninstall = this->clean_button ? true : select_mod_state; // Y键强制卸载，否则根据mod状态决定 (Y key force uninstall, otherwise decide by mod state)

    // 创建回调函数来处理用户确认后的操作 (Create callback function to handle operations after user confirmation)
    auto callback = [this, should_uninstall](bool confirmed) {
        if (confirmed) {
            if (should_uninstall) {
                // 用户确认卸载 (User confirmed uninstall)
                this->ModUninstalled();
            } else {
                // 用户确认安装 (User confirmed install)
                this->ModInstalled();
            }
        } else this->clean_button = false;
        // 如果用户取消，不执行任何操作 (If user cancels, do nothing)
    };

    std::string game_version;
    std::string mod_version;


    // 读取操作 - 安全的锁使用
    {
        std::scoped_lock lock{entries_mutex};
        game_version = this->entries[this->index].display_version;
        mod_version = this->entries[this->index].MOD_VERSION;
    }

    // 比较MOD游戏版本 (Compare MOD game version)
    bool version_pass = CompareModGameVersion(mod_version, game_version);

    std::string confirm_INSTALLED_text = CONFIRM_INSTALLED;
    if(!version_pass && !should_uninstall){
        this->audio_manager.PlayCancelSound();
        confirm_INSTALLED_text = CONFIRM_INSTALLED + VERSION_ERROR_TEXT;
    }

    // 显示确认对话框并传入回调函数 (Show confirmation dialog with callback function)
    if(should_uninstall){
        if (!select_mod_state) {
            this->audio_manager.PlayCancelSound();
            newShowDialogConfirm(GetSnprintf(FORCE_CLEAN_CONFIRM, select_mod_name), callback);
        }
        else newShowDialogConfirm(GetSnprintf(CONFIRM_UNINSTALLED, select_mod_name), callback);
    }else{
        newShowDialogConfirm(GetSnprintf(confirm_INSTALLED_text, select_mod_name), callback);
    }

}



int App::ModInstalled() {
    // 如果有正在运行的任务，检查是否已停止 (If there's a running task, check if it has stopped)
    if (copy_task.valid()) {
        auto status = copy_task.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::timeout) {
            // 任务仍在运行，显示提示对话框 (Task still running, show prompt dialog)
            this->audio_manager.PlayCancelSound();
            newShowDialogConfirm(DNOT_READY);
            return 0; // 返回0表示操作被阻止 (Return 0 to indicate operation blocked)
        }
        copy_task.request_stop(); // 请求停止当前任务 (Request to stop current task)
    }
    
    
    // 检查是否有选中的MOD (Check if there is a selected MOD)
    if (this->mod_info.empty() || this->mod_index >= this->mod_info.size()) {
        return 0; // 没有选中的MOD (No selected MOD)
    }
    
    // 获取当前选中MOD的路径 (Get current selected MOD path)
    std::string mod_path = this->mod_info[this->mod_index].MOD_PATH;
    
    // 立即显示安装进度对话框 (Immediately show installation progress dialog)
    newShowDialogCopyProgress(INSTALLEDING_TEXT, this->mod_info[this->mod_index].MOD_NAME2);
    
    // 初始化进度信息 (Initialize progress info)
    {
        std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
        this->copy_progress = {};
    }
    
    // 启动异步安装任务 (Start async installation task)
    copy_task = util::async([this, mod_path](std::stop_token stop_token) -> bool {
        // 开始计时 (Start timing)
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 创建适配的进度回调函数 (Create adapted progress callback function)
        auto mod_progress_callback = [this](int current, int total, const std::string& current_file, 
                                            bool is_copying_file, float file_progress_percentage,
                                            const std::string& dialog_title, const int* progress_bar_color) {
            CopyProgressInfo progress;
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                progress = this->copy_progress;
            }
            
            // 直接使用传入的参数更新所有进度信息 (Update all progress information with passed parameters)
            progress.files_copied = current;
            progress.total_files = total;
            progress.current_file = current_file;
            progress.progress_percentage = total > 0 ? (float)current / total * 100.0f : 0.0f;
            progress.is_copying_file = is_copying_file;
            progress.file_progress_percentage = file_progress_percentage;
            progress.dialog_title = dialog_title;
            // 使用memcpy复制数组
            std::memcpy(progress.progress_bar_color, progress_bar_color, sizeof(int) * 3);
            
            this->newUpdateCopyProgress(progress);
        };
        
        // 创建错误回调函数 (Create error callback function)
        auto error_callback = [this](const std::string& error_msg) {
            CopyProgressInfo error_progress;
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                error_progress = this->copy_progress;
            }
            error_progress.has_error = true;
            error_progress.error_message = error_msg;
            this->newUpdateCopyProgress(error_progress);
        };
        
        // 使用mod_manager的统一安装接口 (Use mod_manager's unified installation interface)
        bool install_result = this->mod_manager.getModInstallType(
            mod_path,
            1, // operation_type: 1=删除ZIP文件 (1=delete ZIP file)
            mod_progress_callback,
            error_callback,
            stop_token
        );
        
        // 安装完成，计算耗时并更新状态 (Installation completed, calculate duration and update status)
        auto end_time = std::chrono::high_resolution_clock::now();
        this->operation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);


        CopyProgressInfo final_progress;
        {
            std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
            final_progress = this->copy_progress;
        }
        
        final_progress.is_completed = true;
        final_progress.has_error = !install_result;
        // 只有在没有错误信息且安装失败时才设置默认错误信息 (Only set default error message if no error message exists and installation failed)
        if (!install_result && final_progress.error_message.empty()) {
            final_progress.error_message = COPY_ERROR;
        }
        // 如果安装失败但有具体错误信息，保持原有错误信息 (If installation failed but has specific error message, keep original error message)
        this->newUpdateCopyProgress(final_progress);


        return install_result;
    });
    
    return 1; // 返回1表示开始了异步安装 (Return 1 to indicate async installation started)
}


// 卸载选中的MOD从atmosphere目录 (Uninstall selected MOD from atmosphere directory)
int App::ModUninstalled() {
    // 如果有正在运行的任务，检查是否已停止 (If there's a running task, check if it has stopped)
    if (copy_task.valid()) {
        auto status = copy_task.wait_for(std::chrono::milliseconds(0));
        if (status == std::future_status::timeout) {
            // 任务仍在运行，显示提示对话框 (Task still running, show prompt dialog)
            this->audio_manager.PlayCancelSound();
            newShowDialogConfirm(DNOT_READY);
            return 0; // 返回0表示操作被阻止 (Return 0 to indicate operation blocked)
        }
        copy_task.request_stop(); // 请求停止当前任务 (Request to stop current task)
    }
    
    
    // 检查是否有选中的MOD (Check if there is a selected MOD)
    if (this->mod_info.empty() || this->mod_index >= this->mod_info.size()) {
        return 0; // 没有选中的MOD (No selected MOD)
    }
    
    // 获取当前选中MOD的路径 (Get current selected MOD path)
    std::string mod_path = this->mod_info[this->mod_index].MOD_PATH;
    
    // 立即显示卸载进度对话框 (Immediately show uninstall progress dialog)
    newShowDialogCopyProgress(UNINSTALLEDING_TEXT, this->mod_info[this->mod_index].MOD_NAME2);
    // 标志，用于指示卸载是否正在进行 
    this->mod_uninstalling = true;
    
    // 初始化进度信息，总文件数将在异步任务中更新 (Initialize progress info, total files will be updated in async task)
    {
        std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
        this->copy_progress = {};
    }
    
    // 启动异步卸载任务 (Start async uninstall task)
    copy_task = util::async([this, mod_path](std::stop_token stop_token) -> bool {
        // 开始计时 (Start timing)
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 创建进度回调函数 (Create progress callback function)
        auto mod_progress_callback = [this](int current, int total, const std::string& filename, bool is_copying_file, float file_progress_percentage,
                                                const std::string& dialog_title, const int* progress_bar_color) {
            CopyProgressInfo progress;
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                progress = this->copy_progress;
            }
            progress.files_copied = current;
            progress.total_files = total;
            progress.current_file = filename;
            progress.progress_percentage = total > 0 ? (float)current / total * 100.0f : 0.0f;
            progress.dialog_title = dialog_title;
            // 使用memcpy复制数组
            std::memcpy(progress.progress_bar_color, progress_bar_color, sizeof(int) * 3);
            this->newUpdateCopyProgress(progress);
        };
        
        // 创建错误回调函数 (Create error callback function)
        auto error_callback = [this](const std::string& error_message) {
            CopyProgressInfo progress;
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                progress = this->copy_progress;
            }
            progress.error_message = error_message;
            progress.has_error = true;
            this->newUpdateCopyProgress(progress);
        };
        
        // 使用mod_manager的统一卸载接口 (Use mod_manager's unified uninstall interface)
        bool uninstall_result = this->mod_manager.getModInstallType(
            mod_path,
            0, // operation_type: 0=卸载 (0=uninstall)
            mod_progress_callback,
            error_callback,
            stop_token
        );
        
        // 卸载完成，计算耗时并更新状态 (Uninstall completed, calculate duration and update status)
        auto end_time = std::chrono::high_resolution_clock::now();
        this->operation_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        CopyProgressInfo final_progress;
        {
            std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
            final_progress = this->copy_progress;
        }
        final_progress.is_completed = true;
        final_progress.has_error = !uninstall_result;
        if (!uninstall_result && final_progress.error_message.empty()) {
            final_progress.error_message = UNINSTALLED_ERROR;
        }
        this->newUpdateCopyProgress(final_progress);
        
        return uninstall_result;
    });
    
    return 1; // 返回1表示开始了异步卸载 (Return 1 to indicate async uninstall started)
}

// 检查并创建mods2文件夹结构 (Check and create mods2 folder structure)
bool App::CheckMods2Path() {
    // SD卡根目录路径 (SD card root directory path)
    const std::string sd_root = "/";
    
    // mods2文件夹路径 (mods2 folder path)
    const std::string mods2_path = sd_root + "mods2";
    
    // 检查mods2文件夹是否存在 (Check if mods2 folder exists)
    struct stat st;
    if (stat(mods2_path.c_str(), &st) != 0) {
        // mods2文件夹不存在，创建它 (mods2 folder doesn't exist, create it)
        if (mkdir(mods2_path.c_str(), 0755) != 0) {
            // 创建失败 (Creation failed)
            return false;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        // mods2存在但不是文件夹 (mods2 exists but is not a directory)
        return false;
    }
    
    // mods2/add-mod文件夹路径 (mods2/add-mod folder path)
    const std::string add_mod_path = mods2_path + "/0000-add-mod-0000";
    
    // 检查mods2/add-mod文件夹是否存在 (Check if mods2/add-mod folder exists)
    if (stat(add_mod_path.c_str(), &st) != 0) {
        // add-mod文件夹不存在，创建它 (add-mod folder doesn't exist, create it)
        if (mkdir(add_mod_path.c_str(), 0755) != 0) {
            // 创建失败 (Creation failed)
            return false;
        }
    } else if (!S_ISDIR(st.st_mode)) {
        // add-mod存在但不是文件夹 (add-mod exists but is not a directory)
        return false;
    }
    
    // 所有文件夹都存在或创建成功 (All folders exist or were created successfully)
    return true;
}

// 扫描add-mod文件夹中的ZIP文件 (Scan ZIP files in add-mod folder)
std::vector<std::string> App::appendmodscan() {
    std::vector<std::string> zip_files; // 存储ZIP文件名的列表 (List to store ZIP file names)
    
    // add-mod文件夹路径 (add-mod folder path)
    const std::string add_mod_path = "/mods2/0000-add-mod-0000";
    
    // 打开目录 (Open directory)
    DIR* dir = opendir(add_mod_path.c_str());
    if (!dir) {
        // 无法打开目录，返回空容器 (Cannot open directory, return empty vector)
        return zip_files;
    }
    
    struct dirent* entry;
    // 遍历目录中的所有条目 (Iterate through all entries in directory)
    while ((entry = readdir(dir)) != nullptr) {
        // 跳过当前目录和父目录 (Skip current and parent directory entries)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 检查是否为普通文件 (Check if it's a regular file)
        if (entry->d_type == DT_REG) {
            std::string filename = entry->d_name;
            size_t len = filename.length();
            
            // 检查文件扩展名是否为.zip或.ZIP (Check if file extension is .zip or .ZIP)
            if (len > 4) {
                std::string extension = filename.substr(len - 4);
                if (extension == ".zip" || extension == ".ZIP") {
                     zip_files.push_back(filename); // 添加ZIP文件名到列表 (Add ZIP filename to list)
                 }
             }
        }
    }
    
    closedir(dir); // 关闭目录 (Close directory)
    
    // 对ZIP文件名列表进行字典排序 (Sort ZIP file names list alphabetically)
    std::sort(zip_files.begin(), zip_files.end());
    
    return zip_files; // 直接返回ZIP文件名容器 (Return ZIP file names vector directly)
}

// 获取游戏版本ID函数 (Get game version ID function)
// 根据传入的应用ID获取对应的版本ID (Get corresponding version ID based on input application ID)
u32 App::getgameversioninfo(u64 application_id) {
    
    // 获取应用版本信息 (Get application version info)
    AvmVersionListEntry version_entry;
    Result result = avmGetVersionListEntry(application_id, &version_entry);
    
    if (R_SUCCEEDED(result)) {
        // 成功获取版本信息 (Successfully obtained version info)
        return version_entry.version;
    }
    
    // 获取失败时返回一个绝对不可能的版本号 (Return an impossible version number when failed)
    return 0xFFFFFFFE;
}


// =================================================新对话框系统====================================================


// =================================================对话框调配中心====================================================


void App::newDrawDialogBackground() {
    // 绘制对话框背景 (Draw dialog background)

    const float dialog_width = 650.0f;
    const float dialog_height = 350.0f;
    // 对话框位置 (Dialog position)
    const float dialog_x = (SCREEN_WIDTH - dialog_width) / 2.0f;
    const float dialog_y = (SCREEN_HEIGHT - dialog_height) / 2.0f;

    // 绘制半透明背景遮罩 (Draw semi-transparent background overlay)
    gfx::drawRect(this->vg, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, nvgRGBA(0, 0, 0, 128));

    

}



void App::newDrawDialog() {

    // 检查对话框是否可见 (Check if dialog is visible)
    if (!this->show_newdialog) {
        return;
    }

    // 绘制对话框背景 (Draw dialog background)
    this->newDrawDialogBackground();

    // 绘制具体对话框 (Draw dialog content)
    switch (this->newdialog_type) {
        case newDialogType::CONFIRM:
            this->newDrawDialogConfirm();
            break;
        case newDialogType::COPY_PROGRESS:
            this->newDrawDialogCopyProgress();
            break;
        case newDialogType::LIST_SELECT:
            this->newDrawDialogListSelect();
            break;
        case newDialogType::SEARCH:
            this->newDrawDialogSearch();
            break;
        
    } 


}


void App::newUpdateDialog() {
    // 检查对话框是否可见 (Check if dialog is visible)
    if (!this->show_newdialog) {
        return;
    }

    switch (this->newdialog_type) {
        case newDialogType::CONFIRM:
            this->newUpdateDialogConfirm();
            break;
        case newDialogType::COPY_PROGRESS:
            this->newUpdateDialogCopyProgress();
            break;
        case newDialogType::LIST_SELECT:
            this->newUpdateDialogListSelect();
            break;
        case newDialogType::SEARCH:
            this->newUpdateDialogSearch();
            break;
        

    }
} 

void App::newHideDialog() {

    // 隐藏对话框 (Hide dialog)
    this->show_newdialog = false;

    switch (this->newdialog_type) {
        case newDialogType::CONFIRM:
            this->newHideDialogConfirm();
            break;
        case newDialogType::COPY_PROGRESS:
            this->newHideDialogCopyProgress();
            break;
        case newDialogType::LIST_SELECT:
            this->newHideDialogListSelect();
            break;
        case newDialogType::SEARCH:
            this->newHideDialogSearch();
            break;
        
    }
}

// =================================================对话框调配中心结束====================================================


// =================================================搜索对话框===================================================


void App::newDrawDialogSearch() {

    // 纯黑背景
    gfx::drawRect(this->vg, 0.f, 0.f, SCREEN_WIDTH, SCREEN_HEIGHT, gfx::Colour::BLACK);
    // 标题栏分界线
    gfx::drawRect(vg, 30.f, 86.0f, 1220.f, 1.f, gfx::Colour::WHITE);
    // 标题
    gfx::drawText(this->vg, 70.f, 40.f, 28.f, LOADING_TEXT.c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE);

    // 绘制搜索结果区域 (Draw search results area)
    const float search_results_x = 50.f;                     // 搜索结果区域X坐标 (Search results area X coordinate)
    const float search_results_y = 100.f;                    // 搜索结果区域Y坐标 (Search results area Y coordinate)
    const float search_results_width = SCREEN_WIDTH - 100.f; // 搜索结果区域宽度 (Search results area width)
    const float search_results_height = 200.f;               // 搜索结果区域高度 (Search results area height)

    // 绘制搜索结果游戏列表 (3列2行) (Draw search results game list - 3 columns 2 rows)
    if (search_has_results && !search_results.empty()) {
        const float game_item_width = (search_results_width - 50.f) / 3.f;  // 每个游戏项宽度 (Width per game item)
        const float game_item_height = (search_results_height - 30.f) / 2.f; // 每个游戏项高度 (Height per game item)
        const float game_margin = 10.f;                                      // 游戏项间距 (Game item margin)
        
        // 计算垂直偏移量，如果只有1行则居中显示 (Calculate vertical offset, center if only 1 row)
        const bool single_row = search_results.size() <= 3;
        const float vertical_offset = single_row ? game_item_height / 2.f : 0.f;
        
        for (size_t i = 0; i < search_results.size() && i < 6; i++) {
            const int col = i % 3;                                           // 列索引 (Column index)
            const int row = i / 3;                                           // 行索引 (Row index)
            
            const float game_x = search_results_x + game_margin + col * game_item_width;
            const float game_y = search_results_y + game_margin + row * game_item_height + vertical_offset;
            const float game_w = game_item_width - game_margin;
            const float game_h = game_item_height - game_margin;
            
            // 检查是否为当前选中的游戏项 (Check if this is the currently selected game item)
            bool is_selected = (this->search_mode == SearchMode::GAME_LIST && 
                               static_cast<int>(i) == this->search_selected_index);
            
            if (is_selected) {
                // 当前选中项：绘制彩色边框和黑色背景 (Current selected item: draw colored border and black background)
                auto col_pulse = pulse.col;  // 获取脉冲颜色 (Get pulse color)
                col_pulse.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
                col_pulse.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
                col_pulse.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
                col_pulse.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
                update_pulse_colour();       // 更新脉冲颜色(产生闪烁效果) (Update pulse color for blinking effect)
                // 绘制选中项的彩色边框 (Draw colored border for selected item)
                gfx::drawRect(this->vg, game_x - 5.f, game_y - 5.f, game_w + 10.f, game_h + 10.f, col_pulse);
                // 绘制选中项的黑色背景 (Draw black background for selected item)
                gfx::drawRect(this->vg, game_x, game_y, game_w, game_h, gfx::Colour::LIGHT_BLACK);
            } else {
                // 绘制游戏项背景 (Draw game item background)
                gfx::drawRect(this->vg, game_x, game_y, game_w, game_h, gfx::Colour::GREY);
            }
            
            // 绘制游戏映射名称 (Draw game mapped name)
            const std::string display_name = search_results[i].first;  // 获取字符串结果 (Get string result)
            
            nvgSave(this->vg);
            // 设置裁剪区域防止文本溢出到其他格子 (Set clipping area to prevent text overflow to other cells)
            nvgScissor(this->vg, game_x + 10.f, game_y + 5.f, game_w - 20.f, game_h - 10.f);
            nvgFontSize(this->vg, 22.f);
            nvgFillColor(this->vg, nvgRGB(255, 255, 255));
            nvgTextAlign(this->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(this->vg, game_x + game_w / 2.f, game_y + game_h / 2.f, display_name.c_str(), nullptr);
            nvgRestore(this->vg);
        }
    } else {    

        // 绘制提示词 (Draw prompt)
        gfx::drawTextBoxCentered(this->vg, search_results_x + 80.f, search_results_y + 40.f, 
                                search_results_width - 160.f, search_results_height - 80.f, 
                                26.f, 1.4f, 
                                PROMPT_SEARCH_TEXT.c_str(), nullptr, 
                                NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, gfx::Colour::SILVER);
    }
    
    // 绘制矩形：宽为屏幕宽度，高占屏幕3/4，贴屏幕底部，用gfx绘制 (Draw rectangle: width as screen width, height as 3/4 of screen, aligned to bottom, using gfx)
    const float rect_width = SCREEN_WIDTH;                    // 矩形宽度为屏幕宽度 (Rectangle width as screen width)
    const float rect_height = SCREEN_HEIGHT * 0.6f;         // 矩形高度 
    const float rect_x = 0.f;                                // 矩形X坐标为0，贴左边 (Rectangle X coordinate as 0, aligned to left)
    const float rect_y = SCREEN_HEIGHT - rect_height;        // 矩形Y坐标，贴屏幕底部 (Rectangle Y coordinate, aligned to bottom)
    
    // 绘制半透明黑色矩形作为搜索界面背景 (Draw semi-transparent black rectangle as search interface background)
    gfx::drawRect(this->vg, rect_x, rect_y, rect_width, rect_height, gfx::Colour::LIGHT_BLACK);

    // 根据当前模式显示不同的按钮组合 (Display different button combinations based on current mode)
    if (this->search_mode == SearchMode::KEYBOARD) {
        // 虚拟键盘模式的按钮 (Virtual keyboard mode buttons)
        gfx::drawButtonsTop(this->vg, 
                gfx::Colour::WHITE, 
                gfx::pair{gfx::Button::B, DELETE_BUTTON.c_str()},
                gfx::pair{gfx::Button::A, INPUT_BUTTON.c_str()},
                gfx::pair{gfx::Button::X, BUTTON_BACK.c_str()},
                gfx::pair{gfx::Button::Y, RESULT_BUTTON.c_str()});
    } else if (this->search_mode == SearchMode::GAME_LIST) {
        // 游戏列表模式的按钮 (Game list mode buttons)
        gfx::drawButtonsTop(this->vg, 
                gfx::Colour::TEAL, 
                gfx::pair{gfx::Button::B, DELETE_BUTTON.c_str()},
                gfx::pair{gfx::Button::A, BUTTON_SELECT.c_str()},
                gfx::pair{gfx::Button::X, BUTTON_BACK.c_str()},
                gfx::pair{gfx::Button::Y, KEYBOARD_BUTTON.c_str()});

    }
    
    // 绘制虚拟键盘 (Draw virtual keyboard)
    // 键盘布局：QWERTY标准布局，包含26个字母和0-9数字 (Keyboard layout: QWERTY standard layout with 26 letters and 0-9 digits)
    const float keyboard_margin = 40.f;                                    // 键盘边距 (Keyboard margin)
    const float keyboard_x = rect_x + keyboard_margin;                     // 键盘起始X坐标 (Keyboard start X coordinate)
    const float keyboard_y = rect_y + keyboard_margin;                     // 键盘起始Y坐标 (Keyboard start Y coordinate)
    const float keyboard_width = rect_width - 2 * keyboard_margin;         // 键盘总宽度 (Total keyboard width)
    const float keyboard_height = rect_height - 2 * keyboard_margin;       // 键盘总高度 (Total keyboard height)
    
    // 按键尺寸和间距 (Key size and spacing)
    const float key_spacing = 8.f;                                         // 按键间距 (Key spacing)
    const float row_spacing = 12.f;                                        // 行间距 (Row spacing)
    const int keys_per_row[] = {10, 10, 9, 7};                            // 每行按键数量 (Keys per row)
    const int total_rows = 4;                                              // 总行数 (Total rows)
    
    // 计算按键尺寸 (Calculate key size)
    const float key_width = (keyboard_width - (10 - 1) * key_spacing) / 10.f;  // 按键宽度，以第一行10个数字键为基准 (Key width based on first row with 10 digit keys)
    const float key_height = (keyboard_height - (total_rows - 1) * row_spacing) / total_rows;  // 按键高度 (Key height)
    
    // 定义键盘布局 (Define keyboard layout)
    const char* keyboard_layout[4][10] = {
        {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},           // 数字行 (Number row)
        {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},           // 第一字母行 (First letter row)
        {"A", "S", "D", "F", "G", "H", "J", "K", "L", ""},            // 第二字母行 (Second letter row)
        {"Z", "X", "C", "V", "B", "N", "M", "", "", ""}             // 第三字母行 (Third letter row)
    };
    
    // 绘制键盘按键 (Draw keyboard keys)
    for (int row = 0; row < total_rows; row++) {
        const float row_y = keyboard_y + row * (key_height + row_spacing);  // 当前行Y坐标 (Current row Y coordinate)
        const int keys_in_row = keys_per_row[row];                          // 当前行按键数量 (Keys in current row)
        
        // 计算当前行的居中偏移 (Calculate center offset for current row)
        float row_offset = 0.f;
        if (keys_in_row < 10) {
            row_offset = (10 - keys_in_row) * (key_width + key_spacing) / 2.f;
        }
        
        for (int col = 0; col < keys_in_row; col++) {
            const char* key_text = keyboard_layout[row][col];
            if (key_text[0] == '\0') continue;  // 跳过空按键 (Skip empty keys)
            
            const float key_x = keyboard_x + row_offset + col * (key_width + key_spacing);  // 按键X坐标 (Key X coordinate)
            
            // 检查是否为当前选中的按键 (Check if this is the currently selected key)
            // 只在虚拟键盘模式下显示选中框 (Only show selection box in virtual keyboard mode)
            bool is_selected = (this->search_mode == SearchMode::KEYBOARD && 
                               row == this->keyboard_selected_row && 
                               col == this->keyboard_selected_col);
            
            if (is_selected && (!this->left_l_button_selected && !this->right_l_button_selected)) {
                // 当前选中项：绘制彩色边框和黑色背景 (Current selected item: draw colored border and black background)
                auto col_pulse = pulse.col;  // 获取脉冲颜色 (Get pulse color)
                col_pulse.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
                col_pulse.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
                col_pulse.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
                col_pulse.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
                update_pulse_colour();       // 更新脉冲颜色(产生闪烁效果) (Update pulse color for blinking effect)
                // 绘制选中项的彩色边框 (Draw colored border for selected item)
                gfx::drawRect(this->vg, key_x - 5.f, row_y - 5.f, key_width + 10.f, key_height + 10.f, col_pulse);
                // 绘制选中项的黑色背景 (Draw black background for selected item)
                gfx::drawRect(this->vg, key_x, row_y, key_width, key_height, gfx::Colour::BLACK);
            } else {
                // 绘制按键背景 (Draw key background)
                gfx::drawRect(this->vg, key_x, row_y, key_width, key_height, gfx::Colour::DARK_GREY);
                
                // 绘制按键边框 (Draw key border)
                gfx::drawRect(this->vg, key_x, row_y, key_width, 2.f, gfx::Colour::GREY);                    // 顶边 (Top border)
                gfx::drawRect(this->vg, key_x, row_y + key_height - 2.f, key_width, 2.f, gfx::Colour::GREY); // 底边 (Bottom border)
                gfx::drawRect(this->vg, key_x, row_y, 2.f, key_height, gfx::Colour::GREY);                   // 左边 (Left border)
                gfx::drawRect(this->vg, key_x + key_width - 2.f, row_y, 2.f, key_height, gfx::Colour::GREY); // 右边 (Right border)
            }
            
            // 绘制按键文字 (Draw key text)
            const float text_x = key_x + key_width / 2.f;                      // 文字X坐标（居中） (Text X coordinate - centered)
            const float text_y = row_y + key_height / 2.f;                     // 文字Y坐标（居中） (Text Y coordinate - centered)
            const float font_size = 24.f;                                      // 字体大小 (Font size)
            
            gfx::drawText(this->vg, text_x, text_y, font_size, key_text, nullptr, 
                         NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE);
        }
    }

    if (this->left_l_button_selected) { // 左侧L形按钮选中 (Left L-shaped button selected)
        // 当前选中项：绘制彩色边框和黑色背景 (Current selected item: draw colored border and black background)
        auto col_pulse = pulse.col;  // 获取脉冲颜色 (Get pulse color)
        col_pulse.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
        col_pulse.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
        col_pulse.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
        col_pulse.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
        update_pulse_colour();       // 更新脉冲颜色(产生闪烁效果) (Update pulse color for blinking effect)
        // 绘制选中项的彩色边框 (Draw colored border for selected item)
        // 绘制L形按钮边框 (Draw L-shaped button border)
        // 上边框 (Top border)
        gfx::drawRect(this->vg, 35.f, 505.f, 61.6f, 5.f, col_pulse);
        // 右边框1 (Right border 1)
        gfx::drawRect(this->vg, 91.6f, 505.f, 5.f, 96.f, col_pulse);
        // 中间横边框 (Middle horizontal border)
        gfx::drawRect(this->vg, 91.6f, 596.f, 126.6f, 5.f, col_pulse);
        // 右边框2 (Right border 2)
        gfx::drawRect(this->vg, 213.2f, 596.f, 5.f, 89.f, col_pulse);
        // 下边框 (Bottom border)
        gfx::drawRect(this->vg, 35.f, 685.f, 183.2f, 5.f, col_pulse);
        // 左边框 (Left border)
        gfx::drawRect(this->vg, 35.f, 505.f, 5.f, 180.f, col_pulse);
        // 绘制选中项的黑色背景 (Draw black background for selected item)
        nvgBeginPath(this->vg);
        nvgMoveTo(this->vg, 40.f, 510.f);        // 起始点 (Starting point)
        nvgLineTo(this->vg, 91.6f, 510.f);      // 向右 (Move right)
        nvgLineTo(this->vg, 91.6f, 601.f);      // 向下 (Move down)
        nvgLineTo(this->vg, 213.2f, 601.f);     // 向右 (Move right)
        nvgLineTo(this->vg, 213.2f, 680.f);     // 向下 (Move down)
        nvgLineTo(this->vg, 40.f, 680.f);       // 向左 (Move left)
        nvgClosePath(this->vg);                  // 闭合路径回到起始点 (Close path back to starting point)
        nvgFillColor(this->vg, nvgRGB(45, 45, 45)); // 设置填充颜色为灰色 (Set fill color to gray)
        nvgFill(this->vg);                      // 填充多边形 (Fill polygon)

    } else {
        // 绘制按键背景 (Draw key background)
        // 绘制左侧L形按钮 (Draw left L-shaped button)
        nvgBeginPath(this->vg);
        nvgMoveTo(this->vg, 40.f, 510.f);        // 起始点 (Starting point)
        nvgLineTo(this->vg, 91.6f, 510.f);      // 向右 (Move right)
        nvgLineTo(this->vg, 91.6f, 601.f);      // 向下 (Move down)
        nvgLineTo(this->vg, 213.2f, 601.f);     // 向右 (Move right)
        nvgLineTo(this->vg, 213.2f, 680.f);     // 向下 (Move down)
        nvgLineTo(this->vg, 40.f, 680.f);       // 向左 (Move left)
        nvgClosePath(this->vg);                  // 闭合路径回到起始点 (Close path back to starting point)
        nvgFillColor(this->vg, nvgRGB(70, 70, 70)); // 设置填充颜色为灰色 (Set fill color to gray)
        nvgFill(this->vg);                      // 填充多边形 (Fill polygon)
        
        // 绘制按键边框 (Draw key border)
        // 绘制L形按钮边框 (Draw L-shaped button border)
        // 上边框 (Top border)
        gfx::drawRect(this->vg, 40.f, 510.f, 51.6f, 2.f, gfx::Colour::GREY);
        // 右边框1 (Right border 1)
        gfx::drawRect(this->vg, 89.6f, 510.f, 2.f, 91.f, gfx::Colour::GREY);
        // 中间横边框 (Middle horizontal border)
        gfx::drawRect(this->vg, 91.6f, 599.f, 121.6f, 2.f, gfx::Colour::GREY);
        // 右边框2 (Right border 2)
        gfx::drawRect(this->vg, 211.2f, 601.f, 2.f, 79.f, gfx::Colour::GREY);
        // 下边框 (Bottom border)
        gfx::drawRect(this->vg, 40.f, 678.f, 173.2f, 2.f, gfx::Colour::GREY);
        // 左边框 (Left border)
        gfx::drawRect(this->vg, 40.f, 510.f, 2.f, 170.f, gfx::Colour::GREY);
    }

    // 绘制左侧按钮文字"返回" (Draw left button text "Back")
    nvgFontSize(this->vg, 24.f);
    nvgFillColor(this->vg, nvgRGB(255, 255, 255)); // 白色文字 (White text)
    nvgTextAlign(this->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(this->vg, 126.6f, 640.5f, BUTTON_BACK.c_str(), nullptr); // 在L形按钮中心绘制文字 (Draw text at L-shaped button center)


    

    if (this->right_l_button_selected) { // 右侧L形按钮选中
        // 当前选中项：绘制彩色边框和黑色背景 (Current selected item: draw colored border and black background)
        auto col_pulse = pulse.col;  // 获取脉冲颜色 (Get pulse color)
        col_pulse.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
        col_pulse.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
        col_pulse.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
        col_pulse.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
        update_pulse_colour();       // 更新脉冲颜色(产生闪烁效果) (Update pulse color for blinking effect)
        // 绘制选中项的彩色边框 (Draw colored border for selected item)
        // 绘制L形按钮边框 (Draw L-shaped button border)
        // 上边框 (Top border)
        gfx::drawRect(this->vg, 1188.4f - 5.f, 510.f - 5.f, 56.6f, 5.f, col_pulse);
        // 左边框1 (Left border 1)
        gfx::drawRect(this->vg, 1188.4f - 5.f, 510.f - 5.f, 5.f, 96.f, col_pulse);
        // 中间横边框 (Middle horizontal border)
        gfx::drawRect(this->vg, 1066.8f - 5.f, 601.f - 5.f, 126.6f, 5.f, col_pulse); // 中间横边框 (Middle horizontal border)
        // 左边框2 (Left border 2)
        gfx::drawRect(this->vg, 1066.8f - 5.f, 601.f - 5.f, 5.f, 89.f, col_pulse);
        // 下边框 (Bottom border)
        gfx::drawRect(this->vg, 1066.8f - 5.f, 680.f, 183.2f, 5.f, col_pulse);
        // 右边框 (Right border)
        gfx::drawRect(this->vg, 1240.f, 510.f - 5.f, 5.f, 180.f, col_pulse);
        // 绘制选中项的黑色背景 (Draw black background for selected item)
        // 绘制右侧镜像L形按钮 (Draw right mirrored L-shaped button)
        nvgBeginPath(this->vg);
        nvgMoveTo(this->vg, 1240.f, 510.f);        // 起始点 (Starting point)
        nvgLineTo(this->vg, 1188.4f, 510.f);      // 向左 (Move left)
        nvgLineTo(this->vg, 1188.4f, 601.f);      // 向下 (Move down)
        nvgLineTo(this->vg, 1066.8f, 601.f);      // 向左 (Move left)
        nvgLineTo(this->vg, 1066.8f, 680.f);      // 向下 (Move down)
        nvgLineTo(this->vg, 1240.f, 680.f);       // 向右 (Move right)
        nvgClosePath(this->vg);                    // 闭合路径回到起始点 (Close path back to starting point)
        nvgFillColor(this->vg, nvgRGB(45, 45, 45)); // 设置填充颜色为灰色 (Set fill color to gray)
        nvgFill(this->vg);                        // 填充多边形 (Fill polygon)

    } else {
        // 绘制按键背景 (Draw key background)
        // 绘制右侧镜像L形按钮 (Draw right mirrored L-shaped button)
        nvgBeginPath(this->vg);
        nvgMoveTo(this->vg, 1240.f, 510.f);        // 起始点 (Starting point)
        nvgLineTo(this->vg, 1188.4f, 510.f);      // 向左 (Move left)
        nvgLineTo(this->vg, 1188.4f, 601.f);      // 向下 (Move down)
        nvgLineTo(this->vg, 1066.8f, 601.f);      // 向左 (Move left)
        nvgLineTo(this->vg, 1066.8f, 680.f);      // 向下 (Move down)
        nvgLineTo(this->vg, 1240.f, 680.f);       // 向右 (Move right)
        nvgClosePath(this->vg);                    // 闭合路径回到起始点 (Close path back to starting point)
        nvgFillColor(this->vg, nvgRGB(70, 70, 70)); // 设置填充颜色为灰色 (Set fill color to gray)
        nvgFill(this->vg);                        // 填充多边形 (Fill polygon)
        
        // 绘制按键边框 (Draw key border)
        // 绘制L形按钮边框 (Draw L-shaped button border)
        // 上边框 (Top border)
        gfx::drawRect(this->vg, 1188.4f, 510.f, 51.6f, 2.f, gfx::Colour::GREY);
        // 左边框1 (Left border 1)
        gfx::drawRect(this->vg, 1188.4f, 510.f, 2.f, 91.f, gfx::Colour::GREY);
        // 中间横边框 (Middle horizontal border)
        gfx::drawRect(this->vg, 1066.8f, 599.f, 121.6f, 2.f, gfx::Colour::GREY);
        // 左边框2 (Left border 2)
        gfx::drawRect(this->vg, 1066.8f, 601.f, 2.f, 79.f, gfx::Colour::GREY);
        // 下边框 (Bottom border)
        gfx::drawRect(this->vg, 1066.8f, 678.f, 173.2f, 2.f, gfx::Colour::GREY);
        // 右边框 (Right border)
        gfx::drawRect(this->vg, 1238.f, 510.f, 2.f, 170.f, gfx::Colour::GREY);
    }
    
    // 绘制右侧按钮文字"确认" (Draw right button text "Confirm")
    nvgFontSize(this->vg, 24.f);
    nvgFillColor(this->vg, nvgRGB(255, 255, 255)); // 白色文字 (White text)
    nvgTextAlign(this->vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(this->vg, 1153.4f, 640.5f, DELETE_BUTTON.c_str(), nullptr); // 在右侧L形按钮中心绘制文字，Y坐标与Z键一致 (Draw text at right L-shaped button center, Y coordinate aligned with Z key)
 
}


void App::newUpdateDialogSearch() {
    // 调用触控功能处理函数 (Call touch functionality handler)
    newUpdateTouchDialogSearch();
    
    // X键关闭对话框
    if (this->controller.X) {
        this->audio_manager.PlayConfirmSound();
        newHideDialog();
    } else if (this->controller.Y) {
        this->audio_manager.PlayKeySound();
        if (this->search_mode == SearchMode::KEYBOARD) {
            // 从虚拟键盘切换到游戏列表（仅当有搜索结果时） (Switch from virtual keyboard to game list only when there are search results)
            if (this->search_has_results && !this->search_results.empty()) {
                this->search_mode = SearchMode::GAME_LIST;
                // 重置游戏列表选中索引 (Reset game list selected index)
                this->search_selected_index = 0;
            }
        } else {
            // 从游戏列表切换回虚拟键盘 (Switch from game list back to virtual keyboard)
            this->search_mode = SearchMode::KEYBOARD;
        }

        return;
    }
    
    // 定义键盘布局信息 (Define keyboard layout info)
    const int keys_per_row[] = {10, 10, 9, 7};  // 每行按键数量 (Keys per row)
    const int total_rows = 4;                   // 总行数 (Total rows)
    
    // 定义键盘布局 (Define keyboard layout)
    const char* keyboard_layout[4][10] = {
        {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},           // 数字行 (Number row)
        {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},           // 第一字母行 (First letter row)
        {"A", "S", "D", "F", "G", "H", "J", "K", "L", ""},            // 第二字母行 (Second letter row)
        {"Z", "X", "C", "V", "B", "N", "M", "", "", ""}             // 第三字母行 (Third letter row)
    };
    
    // 键盘区域按键处理 (Handle keys based on current mode)
    if (this->search_mode == SearchMode::KEYBOARD) {
        
        // A键输入当前选中的字符 (A key inputs the currently selected character)
        if (this->controller.A) {
            const char* selected_char = keyboard_layout[this->keyboard_selected_row][this->keyboard_selected_col];
            if (selected_char[0] != '\0') {  // 确保不是空字符 (Ensure it's not an empty character)
                if (LOADING_TEXT == SEARCH_TEXT) {
                    LOADING_TEXT = "";
                    
                }
                // 限制最多输入15个字符 (Limit input to maximum 15 characters)
                if (LOADING_TEXT.length() < 25) {
                    this->audio_manager.PlayKeySound();
                    LOADING_TEXT += selected_char;
                    // 输入字符后立即进行搜索 (Search immediately after character input)
                    FilterSearchResults(LOADING_TEXT);
                } else this->audio_manager.PlayLimitSound();
            }
        }
        // B键回退一个字符 (B key backspaces one character)
        else if (this->controller.B && LOADING_TEXT != SEARCH_TEXT) {
            
            if (!LOADING_TEXT.empty()) {
                this->audio_manager.PlayKeySound();
                LOADING_TEXT.pop_back();
                // 回退字符后立即进行搜索 (Search immediately after character backspace)
                FilterSearchResults(LOADING_TEXT);
            } else {
                // 如果搜索文本为空，清空搜索结果 (If search text is empty, clear search results)
                this->audio_manager.PlayLimitSound();
                FilterSearchResults("");
            }
        }
        // 虚拟键盘模式的方向键处理 (Direction key handling for virtual keyboard mode)
        else if (this->controller.UP) {
            
            if (this->keyboard_selected_row > 0) {
                this->audio_manager.PlayKeySound();
                int old_row = this->keyboard_selected_row;
                int old_col = this->keyboard_selected_col;
                this->keyboard_selected_row--;
                
                // 补偿居中偏移导致的列位置错位 (Compensate for column misalignment caused by center offset)
                if (old_row == 3 && this->keyboard_selected_row == 2) {
                    // 从Z-M行(7键,偏移1.5)向上到A-L行(9键,偏移0.5)，需要右移1位对齐 (From Z-M row to A-L row, shift right by 1 to align)
                    this->keyboard_selected_col = old_col + 1;
                } else {
                    this->keyboard_selected_col = old_col;
                }
                
                // 确保列索引不超出新行的范围 (Ensure column index doesn't exceed new row range)
                if (this->keyboard_selected_col >= keys_per_row[this->keyboard_selected_row]) {
                    this->keyboard_selected_col = keys_per_row[this->keyboard_selected_row] - 1;
                }
            } else {
                // 虚拟键盘上键超过边界时，根据当前列位置智能切换到搜索结果列表 (Smart switch to search results based on current column)
                if (this->search_has_results && !this->search_results.empty()) {
                    this->audio_manager.PlayKeySound();
                    this->search_mode = SearchMode::GAME_LIST;
                    int total_items = static_cast<int>(this->search_results.size());
                    
                    // 根据虚拟键盘列位置选择对应的搜索结果项 (Select search result based on keyboard column)
                    auto getColGroup = [](int col) -> int {
                        if (col <= 2) return 0;      // 123列
                        if (col <= 6) return 1;      // 4567列
                        return 2;                     // 890列
                    };
                    
                    int col_group = getColGroup(this->keyboard_selected_col);
                    int target_index;
                    
                    if (total_items <= 3) {
                        // 结果≤3项时，按123位置映射 (When results ≤3, map to 123 positions)
                        target_index = std::min(col_group, total_items - 1);
                    } else {
                        // 结果>3项时，优先选择456位置，无则选最后一项 (When results >3, prefer 456 positions, fallback to last)
                        int preferred_index = col_group + 3;
                        target_index = (total_items > preferred_index) ? preferred_index : (total_items - 1);
                    }
                    
                    this->search_selected_index = target_index;
                } else this->audio_manager.PlayLimitSound();
            }
        }
        else if (this->controller.DOWN) {
            
            if (this->keyboard_selected_row < total_rows - 1) {
                this->audio_manager.PlayKeySound();
                int old_row = this->keyboard_selected_row;
                int old_col = this->keyboard_selected_col;
                this->keyboard_selected_row++;
                
                // 补偿居中偏移导致的列位置错位 (Compensate for column misalignment caused by center offset)
                if (old_row == 2 && this->keyboard_selected_row == 3) {
                    // 从A-L行(9键,偏移0.5)向下到Z-M行(7键,偏移1.5)，需要左移1位对齐 (From A-L row to Z-M row, shift left by 1 to align)
                    this->keyboard_selected_col = old_col - 1;
                    if (this->keyboard_selected_col < 0) {
                        this->keyboard_selected_col = 0;
                    }
                } else {
                    this->keyboard_selected_col = old_col;
                }
                
                // 确保列索引不超出新行的范围 (Ensure column index doesn't exceed new row range)
                if (this->keyboard_selected_col >= keys_per_row[this->keyboard_selected_row]) {
                    this->keyboard_selected_col = keys_per_row[this->keyboard_selected_row] - 1;
                } 
            } else this->audio_manager.PlayLimitSound();
        }
        else if (this->controller.LEFT) {
            this->audio_manager.PlayKeySound();
            // 虚拟键盘左键循环导航 (Virtual keyboard left key circular navigation)
            if (this->keyboard_selected_col > 0) {
                this->keyboard_selected_col--;
            } else {
                // 到达行首时循环到行尾 (Wrap to end of row when reaching beginning)
                this->keyboard_selected_col = keys_per_row[this->keyboard_selected_row] - 1;
            }
        }
        else if (this->controller.RIGHT) {
            this->audio_manager.PlayKeySound();
            // 虚拟键盘右键循环导航 (Virtual keyboard right key circular navigation)
            if (this->keyboard_selected_col < keys_per_row[this->keyboard_selected_row] - 1) {
                this->keyboard_selected_col++;
            } else {
                // 到达行尾时循环到行首 (Wrap to beginning of row when reaching end)
                this->keyboard_selected_col = 0;
            }
        }
        return;
    }

    // 搜索结果列表处理
    if (this->search_mode == SearchMode::GAME_LIST) {
        
        // A键处理
        if (this->controller.A) {
            this->audio_manager.PlayConfirmSound();
            // 获取当前选中的搜索结果 (Get currently selected search result)
            if (this->search_selected_index < static_cast<int>(this->search_results.size())) {
                // 先保存回调函数，防止关闭对话框时被清理 (Save callback first to prevent cleanup when closing dialog)
                auto callback = this->newdialog_search_callback;
                if (callback) {
                    int selected_index = this->search_results[this->search_selected_index].second;
                    callback(selected_index);
                }
                // 关闭搜索对话框 (Close search dialog)
                newHideDialog();
            }
        }
        // 搜索结果列表导航
        if (this->controller.UP) {
            
            // 向上移动3个位置（跨行） (Move up 3 positions to cross rows)
            if (this->search_selected_index >= 3) {
                this->audio_manager.PlayKeySound();
                this->search_selected_index -= 3;
            } else this->audio_manager.PlayLimitSound();
        } else if (this->controller.DOWN) {
            this->audio_manager.PlayKeySound();
            // 向下移动3个位置（跨行） (Move down 3 positions to cross rows)
            if (this->search_selected_index + 3 < static_cast<int>(this->search_results.size())) {
                this->search_selected_index += 3;
            } else {
                // 搜索结果列表下键边界处理 (Handle DOWN key boundary for search results)
                auto getKeyboardColFromIndex = [](int index) -> int {
                    static const int cols[] = {1, 4, 8}; // 对应数字2、5、9的列位置
                    return cols[index % 3];
                };
                
                if (this->search_selected_index < 3) {
                    // 123项按下处理 (Handle items 123 pressing down)
                    int total_items = static_cast<int>(this->search_results.size());
                    if (total_items > 3) {
                        // 有第二行，优先切换到对应位置或最后一项 (Has second row, switch to corresponding or last item)
                        int target_index = this->search_selected_index + 3;
                        this->search_selected_index = (target_index < total_items) ? target_index : (total_items - 1);
                    } else {
                        // 只有123项，切换到虚拟键盘对应位置 (Only 123 items, switch to keyboard)
                        this->search_mode = SearchMode::KEYBOARD;
                        this->keyboard_selected_row = 0;
                        this->keyboard_selected_col = getKeyboardColFromIndex(this->search_selected_index);
                    }
                } else {
                    // 456项切换到虚拟键盘 (Items 456 switch to keyboard)
                    this->search_mode = SearchMode::KEYBOARD;
                    this->keyboard_selected_row = 0;
                    this->keyboard_selected_col = getKeyboardColFromIndex(this->search_selected_index);
                }
            }
        } else if (this->controller.LEFT) {
            this->audio_manager.PlayKeySound();
            // 向左移动，支持循环换行 (Move left with circular line wrapping)
            if (this->search_selected_index % 3 > 0) {
                // 在行内向左移动 (Move left within the row)
                this->search_selected_index--;
            } else if (this->search_selected_index > 0) {
                // 在行首时跳到上一行的末尾 (Jump to end of previous row when at row start)
                int prev_row_start = this->search_selected_index - 3;
                int prev_row_end = std::min(prev_row_start + 2, static_cast<int>(this->search_results.size()) - 1);
                this->search_selected_index = prev_row_end;
            } else {
                // 在第一个格子时跳到最后一个格子 (Jump to last item when at first item)
                this->search_selected_index = static_cast<int>(this->search_results.size()) - 1;
            }
        } else if (this->controller.RIGHT) {
            this->audio_manager.PlayKeySound();
            // 向右移动，支持循环换行 (Move right with circular line wrapping)
            if (this->search_selected_index % 3 < 2 && this->search_selected_index + 1 < static_cast<int>(this->search_results.size())) {
                // 在行内向右移动 (Move right within the row)
                this->search_selected_index++;
            } else {
                // 在行尾或最后一个项目时，检查是否有下一行 (At row end or last item, check if next row exists)
                int next_row_start = (this->search_selected_index / 3 + 1) * 3;
                if (next_row_start < static_cast<int>(this->search_results.size())) {
                    // 跳到下一行的开头 (Jump to start of next row)
                    this->search_selected_index = next_row_start;
                } else {
                    // 跳到第一个格子 (Jump to first item)
                    this->search_selected_index = 0;
                }
            }
        }

        return;
    }

}


void App::newHideDialogSearch() {

    // 清空搜索相关状态，但保持搜索框内容 (Clear search-related state but keep search box content)
    this->search_results.clear();
    this->search_has_results = false;
    this->search_selected_index = 0;
    this->newdialog_search_callback = nullptr;
    this->search_context.clear();
    this->right_l_button_selected = false;
    this->left_l_button_selected = false;

}

void App::newUpdateTouchDialogSearch() {
    
    // 处理右侧L形按钮持续触发逻辑 (Handle right L-shaped button continuous trigger logic)
    if (this->right_l_button_held) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed_since_start = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->right_l_button_press_start).count();
        auto elapsed_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->right_l_button_last_trigger).count();
        
        // 检查是否应该重复触发 (Check if should repeat trigger)
        bool should_trigger = false;
        if (elapsed_since_start >= RIGHT_L_BUTTON_INITIAL_DELAY_MS && 
            elapsed_since_last >= RIGHT_L_BUTTON_REPEAT_INTERVAL_MS) {
            should_trigger = true;
        }
        
        if (should_trigger) {
            this->right_l_button_last_trigger = now;
            
            // 执行删除操作 (Execute delete operation)
            if (!LOADING_TEXT.empty() && LOADING_TEXT != SEARCH_TEXT) {
                this->audio_manager.PlayKeySound();
                LOADING_TEXT.pop_back();
                // 回退字符后立即进行搜索 (Search immediately after character backspace)
                FilterSearchResults(LOADING_TEXT);
            } else {
                // 如果搜索文本为空，清空搜索结果 (If search text is empty, clear search results)
                this->audio_manager.PlayLimitSound();
                FilterSearchResults("");
            }
        }
    }
    
    // 检查延迟回调执行 (Check delayed callback execution)
    if (this->pending_search_callback) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_frame_time).count();
        if (elapsed >= 100) { // 100ms延迟 (100ms delay)
            this->pending_search_callback = false;
            
            if (!this->left_l_button_selected){ //如果为真则代表是用来延迟返回按钮的
                // 执行回调函数 (Execute callback function)
                auto callback = this->newdialog_search_callback;
                if (callback && this->search_selected_index >= 0 && this->search_selected_index < this->search_results.size()) {
                    int selected_index = this->search_results[this->search_selected_index].second;
                    callback(selected_index);
                }
            }
            // 关闭搜索对话框 (Close search dialog)
            this->newHideDialog();
            return; // 执行回调后直接返回，不继续处理触屏输入 (Return directly after callback execution, don't process touch input)
        }
    }
    
    // 获取触摸状态 (Get touch states)
    HidTouchScreenState touch_states[1];
    size_t touch_count = hidGetTouchScreenStates(touch_states, 1);
    
    // 检查当前触摸状态 (Check current touch state)
    bool current_touch_pressed = false;
    float touch_x = 0.0f;
    float touch_y = 0.0f;
    
    if (touch_count > 0) {
        const HidTouchScreenState& latest_touch = touch_states[touch_count - 1];
        if (latest_touch.count > 0) {
            current_touch_pressed = true;
            const HidTouchState& touch = latest_touch.touches[0];
            touch_x = static_cast<float>(touch.x);
            touch_y = static_cast<float>(touch.y);
        }
    }
    
    // 触摸状态机逻辑 (Touch state machine logic)
    // 检测触摸按下事件：当前按下且上一帧未按下且未锁定 (Detect touch press: currently pressed, not pressed last frame, and not locked)
    bool touch_just_pressed = current_touch_pressed && !this->last_touch_pressed && !this->touch_locked;
    
    // 检测触摸松开事件：当前未按下且上一帧按下 (Detect touch release: currently not pressed but was pressed last frame)
    bool touch_just_released = !current_touch_pressed && this->last_touch_pressed;
    
    // 更新上一帧触摸状态 (Update last frame touch state)
    this->last_touch_pressed = current_touch_pressed;
    
    // 处理触摸松开事件：解锁触摸 (Handle touch release: unlock touch)
    if (touch_just_released) {
        this->touch_locked = false;
        // 重置右侧L形按钮状态 (Reset right L-shaped button state)
        this->right_l_button_held = false;
        return; // 松开时不处理点击逻辑，只解锁 (Don't process click logic on release, just unlock)
    }
    
    // 如果没有触摸按下事件或触摸已锁定，直接返回 (Return if no touch press event or touch is locked)
    if (!touch_just_pressed) {
        // 如果当前没有触摸且右侧L形按钮被标记为按住状态，重置状态 (If no current touch and right L-shaped button is marked as held, reset state)
        if (!current_touch_pressed && this->right_l_button_held) {
            this->right_l_button_held = false;
        }
        return;
    }
    
    // 触摸按下时立即锁定 (Lock touch immediately when pressed)
    this->touch_locked = true;
    
    // 检查左侧L形按钮触摸 (Check left L-shaped button touch)
    // 检查右侧L形按钮触摸 (Check right L-shaped button touch)
    // 由两个矩形区域组成 (Right L-shaped button consists of two rectangular areas)
    if ((touch_x >= 40.f && touch_x <= 91.6f && touch_y >= 510.f && touch_y <= 601.f) ||  // 上部分 (Upper part)
        (touch_x >= 40.f && touch_x <= 213.2f && touch_y >= 601.f && touch_y <= 680.f)) {  // 下部分 (Lower part)
        this->left_l_button_selected = true;
        this->right_l_button_selected = false;
        this->audio_manager.PlayConfirmSound();
        // 设置延迟回调标志，复用last_frame_time计时 (Set delayed callback flag, reuse last_frame_time for timing)
        if (this->newdialog_search_callback) {
            this->pending_search_callback = true;
            this->last_frame_time = std::chrono::steady_clock::now();
        }
        return;
    }
    else if ((touch_x >= 1188.4f && touch_x <= 1240.f && touch_y >= 510.f && touch_y <= 601.f) ||  // 上部分 (Upper part)
        (touch_x >= 1066.8f && touch_x <= 1240.f && touch_y >= 601.f && touch_y <= 680.f)) {  // 下部分 (Lower part)
        
        this->left_l_button_selected = false;
        this->right_l_button_selected = true;
        
        // 检查是否是新的按下事件 (Check if this is a new press event)
        if (!this->right_l_button_held) {
            this->right_l_button_held = true;
            this->right_l_button_press_start = std::chrono::steady_clock::now();
            this->right_l_button_last_trigger = this->right_l_button_press_start;
            
            // 立即执行一次删除操作 (Execute delete operation immediately)
            if (!LOADING_TEXT.empty() && LOADING_TEXT != SEARCH_TEXT) {
                this->audio_manager.PlayKeySound();
                LOADING_TEXT.pop_back();
                // 回退字符后立即进行搜索 (Search immediately after character backspace)
                FilterSearchResults(LOADING_TEXT);
            } else {
                // 如果搜索文本为空，清空搜索结果 (If search text is empty, clear search results)
                this->audio_manager.PlayLimitSound();
                FilterSearchResults("");
            }
        }
        return;
    }
    
    // 定义搜索结果区域坐标 (Define search results area coordinates)
    const float search_results_x = 50.f;
    const float search_results_y = 100.f;
    const float search_results_width = SCREEN_WIDTH - 100.f;
    const float search_results_height = 200.f;
    
    // 检查是否点击了搜索结果区域 (Check if search results area was clicked)
    if (this->search_has_results && !this->search_results.empty() &&
        touch_x >= search_results_x && touch_x <= search_results_x + search_results_width &&
        touch_y >= search_results_y && touch_y <= search_results_y + search_results_height) {

        // 切换到虚拟键盘模式 (Switch to virtual keyboard mode)
        this->search_mode = SearchMode::GAME_LIST;
        
        // 计算游戏项布局参数 (Calculate game item layout parameters)
        const float game_item_width = (search_results_width - 50.f) / 3.f;
        const float game_item_height = (search_results_height - 30.f) / 2.f;
        const float game_margin = 10.f;
        
        // 计算垂直偏移量 (Calculate vertical offset)
        const bool single_row = search_results.size() <= 3;
        const float vertical_offset = single_row ? game_item_height / 2.f : 0.f;
        
        // 检测点击了哪个游戏项 (Detect which game item was clicked)
        for (size_t i = 0; i < search_results.size() && i < 6; i++) {
            const int col = i % 3;
            const int row = i / 3;
            
            const float game_x = search_results_x + game_margin + col * game_item_width;
            const float game_y = search_results_y + game_margin + row * game_item_height + vertical_offset;
            const float game_w = game_item_width - game_margin;
            const float game_h = game_item_height - game_margin;
            
            // 检查触摸点是否在当前游戏项范围内 (Check if touch point is within current game item)
            if (touch_x >= game_x && touch_x <= game_x + game_w &&
                touch_y >= game_y && touch_y <= game_y + game_h) {
                
                // 播放确认音效 (Play confirmation sound)
                this->audio_manager.PlayConfirmSound();
                
                // 更新选中索引 (Update selected index)
                this->search_selected_index = static_cast<int>(i);
                
                // 设置延迟回调标志，复用last_frame_time计时 (Set delayed callback flag, reuse last_frame_time for timing)
                if (this->newdialog_search_callback) {
                    this->pending_search_callback = true;
                    this->last_frame_time = std::chrono::steady_clock::now();
                }
                
                // 注意：不立即关闭对话框，让Update函数在延迟后处理关闭和回调 (Note: Don't close dialog immediately, let Update function handle closing and callback after delay)
                return;
            }
        }
        return;
    }
    
    // 定义虚拟键盘区域坐标 (Define virtual keyboard area coordinates)
    const float rect_width = SCREEN_WIDTH;
    const float rect_height = SCREEN_HEIGHT * 0.6f;
    const float rect_x = 0.f;
    const float rect_y = SCREEN_HEIGHT - rect_height;
    
    // 检查是否点击了虚拟键盘区域 (Check if virtual keyboard area was clicked)
    if (touch_x >= rect_x && touch_x <= rect_x + rect_width &&
        touch_y >= rect_y && touch_y <= rect_y + rect_height) {
        
        // 计算键盘布局参数 (Calculate keyboard layout parameters)
        const float keyboard_margin = 40.f;
        const float keyboard_x = rect_x + keyboard_margin;
        const float keyboard_y = rect_y + keyboard_margin;
        const float keyboard_width = rect_width - 2 * keyboard_margin;
        const float keyboard_height = rect_height - 2 * keyboard_margin;
        
        const float key_spacing = 8.f;
        const float row_spacing = 12.f;
        const int keys_per_row[] = {10, 10, 9, 7};
        const int total_rows = 4;
        
        const float key_width = (keyboard_width - (10 - 1) * key_spacing) / 10.f;
        const float key_height = (keyboard_height - (total_rows - 1) * row_spacing) / total_rows;
        
        // 定义键盘布局 (Define keyboard layout)
        const char* keyboard_layout[4][10] = {
            {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"},
            {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
            {"A", "S", "D", "F", "G", "H", "J", "K", "L", ""},
            {"Z", "X", "C", "V", "B", "N", "M", "", "", ""}
        };
        
        // 检测点击了哪个按键 (Detect which key was clicked)
        for (int row = 0; row < total_rows; row++) {
            const float row_y = keyboard_y + row * (key_height + row_spacing);
            const int keys_in_row = keys_per_row[row];
            
            // 计算当前行的居中偏移 (Calculate center offset for current row)
            float row_offset = 0.f;
            if (keys_in_row < 10) {
                row_offset = (10 - keys_in_row) * (key_width + key_spacing) / 2.f;
            }
            
            for (int col = 0; col < keys_in_row; col++) {
                const char* key_text = keyboard_layout[row][col];
                if (key_text[0] == '\0') continue;
                
                const float key_x = keyboard_x + row_offset + col * (key_width + key_spacing);
                
                // 检查触摸点是否在当前按键范围内 (Check if touch point is within current key)
                if (touch_x >= key_x && touch_x <= key_x + key_width &&
                    touch_y >= row_y && touch_y <= row_y + key_height) {
                    
                    // 切换到虚拟键盘模式 (Switch to virtual keyboard mode)
                    this->search_mode = SearchMode::KEYBOARD;
                    this->left_l_button_selected = false;
                    this->right_l_button_selected = false;
                    
                    // 更新选中的按键位置 (Update selected key position)
                    this->keyboard_selected_row = row;
                    this->keyboard_selected_col = col;
                    
                    // 处理按键输入 (Handle key input)
                    if (LOADING_TEXT == SEARCH_TEXT) {
                        LOADING_TEXT = "";
                    }
                    
                    // 限制最多输入25个字符 (Limit input to maximum 25 characters)
                    if (LOADING_TEXT.length() < 25) {
                        this->audio_manager.PlayKeySound();
                        LOADING_TEXT += key_text;
                        // 输入字符后立即进行搜索 (Search immediately after character input)
                        FilterSearchResults(LOADING_TEXT);
                    } else {
                        this->audio_manager.PlayLimitSound();
                    }
                    
                    return;
                }
            }
        }
    }
}


void App::newShowDialogSearch(std::vector<std::string>& search_context, DialogSearchCallback callback) {
    // 显示确认对话框 (Show confirm dialog)
    this->newdialog_type = newDialogType::SEARCH;                                     // 搜索对话框类型 (Confirm dialog type)
    this->search_mode = SearchMode::KEYBOARD;
    this->show_newdialog = true;                                                       // 是否显示确认对话框 (Whether to show confirm dialog)
    // 初始化虚拟键盘选中状态 (Initialize virtual keyboard selection state)
    this->keyboard_selected_row = 1;
    this->keyboard_selected_col = 4;
    LOADING_TEXT = SEARCH_TEXT;
    this->search_context = search_context;
    this->newdialog_search_callback= callback;
}



// =================================================搜索对话框结束===================================================

// =================================================确认型对话框====================================================




void App::newDrawDialogConfirm() {

    const float dialog_width = 650.0f;
    const float dialog_height = 350.0f;
    // 对话框位置 (Dialog position)
    const float dialog_x = (SCREEN_WIDTH - dialog_width) / 2.0f;
    const float dialog_y = (SCREEN_HEIGHT - dialog_height) / 2.0f;

    // 按钮尺寸和位置 (Button size and position)
    const float button_height = 75.0f;
    // 计算布局参数 (Calculate layout parameters)
    const float content_height = dialog_height - button_height; // 内容区域高度 (Content area height)
    const float button_width = dialog_width / 2.0f; // 每个按钮占对话框一半宽度 (Each button takes half dialog width)
    const float buttons_y = dialog_y + dialog_height - button_height; // 按钮Y坐标 (Button Y coordinate)


    // 绘制对话框背景 (Draw dialog background)
    gfx::drawRoundedRect(this->vg, dialog_x, dialog_y, dialog_width, dialog_height, 6.0f, 45, 45, 45);

    // 绘制内容 (Draw content) - 使用drawTextBoxCentered在除按钮外的全部区域居中显示文本
    gfx::drawTextBoxCentered(this->vg, dialog_x + 70.f, dialog_y + 40.f, dialog_width - 140.f, content_height - 80.f, 
                             this->newdialog_content_font_size, 1.4f, 
                             this->newdialog_content.c_str(), nullptr, 
                             this->newdialog_content_align, gfx::Colour::WHITE);
    
    
    
    // 先绘制所有按钮的基础背景 (First draw all button backgrounds)
    // 确定按钮背景 (OK button background)
    gfx::drawRoundedRectVarying(this->vg, dialog_x, buttons_y, button_width, button_height, 
                                0.0f, 0.0f, 0.0f, 6.0f, 45, 45, 45); // 左半部分，左下角圆角 (Left half, bottom-left corner rounded)
    
    // 取消按钮背景 (Cancel button background)
    gfx::drawRoundedRectVarying(this->vg, dialog_x + button_width, buttons_y, button_width, button_height, 
                                0.0f, 0.0f, 6.0f, 0.0f, 45, 45, 45); // 右半部分，右下角圆角 (Right half, bottom-right corner rounded)
    
    // 然后绘制选中状态的彩色边框 (Then draw selected state colored borders)
    auto col = pulse.col;  // 获取脉冲颜色 (Get pulse color)
    col.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
    col.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
    col.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
    col.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
    // 更新脉冲颜色动画 (Update pulse color animation)
    update_pulse_colour();

    // 先绘制按钮分界线 (Draw button divider lines first)
    // 绘制按钮顶部分界线 (Draw top divider line)
    gfx::drawRect(this->vg, dialog_x, buttons_y, dialog_width, 2.0f, nvgRGB(100, 100, 100));
    
    // 绘制按钮中间分界线 (Draw middle divider line)
    gfx::drawRect(this->vg, dialog_x + button_width, buttons_y, 2.0f, button_height, nvgRGB(100, 100, 100));

    if (this->newdialog_button) {
        // 确定按钮选中：绘制彩色边框 (OK button selected: draw colored border)
        gfx::drawRoundedRect(this->vg, dialog_x - 3.f, buttons_y - 3.f, button_width + 6.f, button_height + 6.f, 6.0f, 
                            (int)(col.r * 255), (int)(col.g * 255), (int)(col.b * 255));
        // 重新绘制确定按钮的黑色背景 (Redraw OK button black background)
        gfx::drawRoundedRectVarying(this->vg, dialog_x, buttons_y, button_width, button_height, 
                                   0.0f, 0.0f, 0.0f, 6.0f, 45, 45, 45);
    } else {
        // 取消按钮选中：绘制彩色边框 (Cancel button selected: draw colored border)
        gfx::drawRoundedRect(this->vg, dialog_x + button_width - 3.f, buttons_y - 3.f, button_width + 6.f, button_height + 6.f, 6.0f, 
                            (int)(col.r * 255), (int)(col.g * 255), (int)(col.b * 255));
        // 重新绘制取消按钮的黑色背景 (Redraw Cancel button black background)
        gfx::drawRoundedRectVarying(this->vg, dialog_x + button_width, buttons_y, button_width, button_height, 
                                   0.0f, 0.0f, 6.0f, 0.0f, 45, 45, 45);
    }
    
    // 绘制按钮文字 (Draw button text)
    // 确定按钮文字 (OK button text)
    gfx::drawText(this->vg, dialog_x + button_width / 2.0f, buttons_y + button_height / 2.0f, 24.0f, 
                  OK_BUTTON.c_str(), nullptr, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, nvgRGB(255, 255, 255));
    
    // 取消按钮文字 (Cancel button text)
    gfx::drawText(this->vg, dialog_x + button_width + button_width / 2.0f, buttons_y + button_height / 2.0f, 24.0f, 
                  BUTTON_CANCEL.c_str(), nullptr, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, nvgRGB(255, 255, 255));


}



void App::newUpdateDialogConfirm() {
    // 更新确认对话框 (Update confirm dialog)

    if (this->controller.B) {
        // 用户按下B键取消 (User pressed B to cancel)
        this->audio_manager.PlayConfirmSound(1.0);
        this->newdialog_button = false;
        
        // 调用回调函数，传递取消结果 (Call callback function with cancel result)
        if (this->newdialog_callback) {
            auto callback = this->newdialog_callback; // 保存回调函数 (Save callback function)
            this->newHideDialog(); // 先隐藏对话框 (Hide dialog first)
            callback(false); // 然后调用回调 (Then call callback)
        } else {
            this->newHideDialog();
        }

    }else if (this->controller.A) {
        // 用户按下A键确认 (User pressed A to confirm)
        this->audio_manager.PlayConfirmSound(1.0);
        bool confirmed = this->newdialog_button; // 保存确认状态 (Save confirmation state)

        // 调用回调函数，传递确认结果 (Call callback function with confirmation result)
        if (this->newdialog_callback) {
            auto callback = this->newdialog_callback; // 保存回调函数 (Save callback function)
            this->newHideDialog(); // 先隐藏对话框 (Hide dialog first)
            callback(confirmed); // 然后调用回调 (Then call callback)
        } else {
            this->newHideDialog();
        }

    }else if (this->controller.LEFT) {
        // 如果当前选中是确认按钮，则触发边界音效，否则切换到取消按钮
        if (this->newdialog_button) {
            this->audio_manager.PlayLimitSound(1.0);
        } else {
            this->audio_manager.PlayKeySound(1.0);
            this->newdialog_button = !this->newdialog_button;
        }
        
        
    }else if (this->controller.RIGHT) {
        // 如果当前选中是取消按钮，则触发边界音效，否则切换到确认按钮
        if (!this->newdialog_button) {
            this->audio_manager.PlayLimitSound(1.0);  
        } else {
            this->newdialog_button = !this->newdialog_button;
            this->audio_manager.PlayKeySound(1.0); 
        }
        
    }

}

void App::newShowDialogConfirm(const std::string& newdialog_content,
                                DialogConfirmCallback callback,
                                float newdialog_content_font_size,
                                int newdialog_content_align) {
    // 显示确认对话框 (Show confirm dialog)
    this->newdialog_type = newDialogType::CONFIRM;                                     // 确认对话框类型 (Confirm dialog type)
    this->show_newdialog = true;                                                       // 是否显示确认对话框 (Whether to show confirm dialog)
    this->newdialog_button = false;                                                   // 确认对话框结果 (Confirm dialog result)
    this->newdialog_content = newdialog_content;                                      // 文本内容
    this->newdialog_content_font_size = newdialog_content_font_size;                  // 字体大小
    this->newdialog_content_align = newdialog_content_align;                           //文本位置
    this->newdialog_callback = callback;                                              // 设置回调函数 (Set callback function)
}

void App::newHideDialogConfirm() {

    // 隐藏确认对话框 (Hide confirm dialog)
    this->newdialog_content = "";
    this->newdialog_callback = nullptr;
    

}


// =================================================确认型对话框结束====================================================


// =================================================列表对话框====================================================

void App::newDrawDialogListSelect(){
    // 绘制列表选择对话框 (Draw list select dialog)
    
    const float list_dialog_width = SCREEN_WIDTH / 3;                   // 列表对话框宽度 (List dialog width)
    const float list_dialog_height = SCREEN_HEIGHT;                       // 列表对话框高度 (List dialog height)
    const float list_dialog_x = SCREEN_WIDTH - list_dialog_width;         // 列表对话框x坐标 (List dialog x position)
    const float list_dialog_y = 0;                                        // 列表对话框y坐标 (List dialog y position)
    const float list_item_height = 80.f;                                  // 列表项高度 (List item height)
    const float offset_to_right = 40.f;

    // 绘制对话框背景 (Draw dialog background)
    gfx::drawRect(this->vg, list_dialog_x, list_dialog_y, list_dialog_width, list_dialog_height, gfx::Colour::LIGHT_BLACK);
    // 绘制顶部分割线 (Draw top divider line)
    gfx::drawRect(this->vg, list_dialog_x, 86.0f, list_dialog_width, 1.f, gfx::Colour::WHITE);
    
    // 绘制底部分割线 (Draw bottom divider line)
    gfx::drawRect(this->vg, list_dialog_x, 646.0f, list_dialog_width, 1.f, gfx::Colour::WHITE);

    gfx::drawTextWithButtonMarker(this->vg, list_dialog_x +40.f, 40.f, 28.f, this->newdialog_list_title.c_str(), nullptr, NVG_ALIGN_LEFT | NVG_ALIGN_TOP, gfx::Colour::WHITE);




    // 如果传入列表为空。且只有MOD选择框会是空的。
    if (this->newdialog_list_items.empty() && this->multiple_select_mode) {
            gfx::drawButtons(this->vg, 
                gfx::Colour::WHITE, 
                gfx::pair{gfx::Button::B, BUTTON_CANCEL.c_str()});

            gfx::drawTextBoxCentered(this->vg, list_dialog_x + 40.f, list_dialog_y, list_dialog_width - 80.f, list_dialog_height, 28.f, 2.0f, 
                            NO_FOUND_MOD_ERROR.c_str(), nullptr, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, gfx::Colour::SILVER);
            return;
    }

    gfx::drawButtons(this->vg, 
            gfx::Colour::WHITE, 
            gfx::pair{gfx::Button::B, BUTTON_CANCEL.c_str()},
            gfx::pair{gfx::Button::A, BUTTON_SELECT.c_str()});

    // 计算当前页显示的列表项范围 (Calculate current page display range)
    const size_t items_per_page = 6;
    const size_t start_index = this->newdialog_list_start_index;
    const size_t end_index = std::min(start_index + items_per_page, this->newdialog_list_items.size());

    for (size_t i = start_index; i < end_index; ++i) {

        const float item_y = 165.f + (i - start_index) * list_item_height; // 使用相对位置计算Y坐标 (Use relative position to calculate Y coordinate)
        
        // 检查是否为当前光标选中的项 (Check if this is the currently cursor-selected item)
        if (i == this->newdialog_list_selected_index) {
            // 当前选中项：绘制彩色边框和黑色背景 (Current selected item: draw colored border and black background)
            auto col = pulse.col;  // 获取脉冲颜色 (Get pulse color)
            col.r /= 255.f;        // 红色通道归一化(0-1范围) (Normalize red channel to 0-1 range)
            col.g /= 255.f;        // 绿色通道归一化 (Normalize green channel)
            col.b /= 255.f;        // 蓝色通道归一化 (Normalize blue channel)
            col.a = 1.f;           // 设置不透明度为1(完全不透明) (Set alpha to 1 - fully opaque)
            update_pulse_colour(); // 更新脉冲颜色(产生闪烁效果) (Update pulse color for blinking effect)
            // 绘制选中项的彩色边框 (Draw colored border for selected item)
            gfx::drawRect(this->vg, list_dialog_x + offset_to_right, item_y - list_item_height / 2, 
                            list_dialog_width - offset_to_right * 2, list_item_height, col);
            // 绘制选中项的黑色背景 (Draw black background for selected item, smaller than border)
            gfx::drawRect(this->vg, list_dialog_x + offset_to_right + 4.0f, item_y + 4.0f - list_item_height / 2, 
                            list_dialog_width - offset_to_right * 2 - 8.0f, 
                            list_item_height - 8.0f, gfx::Colour::BLACK);

        }

        // 绘制列表项文本 (Draw list item text) - 使用裁剪区域防止文本超出选中框
        const float text_max_width = list_dialog_width - offset_to_right * 2 - 45.0f; // 文本最大宽度，留出边距 (Max text width with margin)
        
        // 保存当前绘图状态并设置文本裁剪区域 (Save current drawing state and set text clipping area)
        nvgSave(this->vg);
        nvgScissor(this->vg, list_dialog_x + 60.f, item_y - list_item_height / 2, text_max_width, list_item_height);
        
        // 根据对话框类型和选中状态决定文本颜色 (Determine text color based on dialog type and selection state)
        gfx::Colour text_color = gfx::Colour::WHITE; // 默认白色 (Default white)
        if (this->multiple_select_mode && this->newdialog_list_selected_indices.count(i)) {
            // 多选模式下已选中的项显示青色 (Selected items in multi-select mode show cyan color)
            text_color = gfx::Colour::TEAL;
        }
        
        gfx::drawText(this->vg, list_dialog_x + 60.f, item_y, this->newdialog_content_font_size,
                        this->newdialog_list_items[i].c_str(), nullptr, 
                        NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, text_color);
        
        // 恢复之前保存的绘图状态 (Restore previously saved drawing state)
        nvgRestore(this->vg);

        // 绘制列表项之间的分界线 (Draw divider line between list items)
        if (i < end_index - 1) { // 不为当前页最后一个列表项绘制分界线 (Don't draw divider for the last item in current page)
            // // 绘制分界线
            gfx::drawRect(this->vg, list_dialog_x + offset_to_right, item_y + list_item_height / 2, 
                            list_dialog_width - offset_to_right * 2, 1.f, gfx::Colour::DARK_GREY);

        }
    }

    // 绘制滑动条 (Draw scrollbar) - 当列表项超过6个时显示
    if (this->newdialog_list_items.size() > items_per_page) {
        const float scrollbar_width = 4.0f;  // 滑动条宽度 (Scrollbar width) - 改为原来的一半
        const float scrollbar_x = list_dialog_x + list_dialog_width - scrollbar_width - 10.0f; // 滑动条X坐标 (Scrollbar X position)
        const float scrollbar_area_y = 86.0f + 10.0f; // 滑动条区域起始Y坐标 - 距离上分界线10像素 (Scrollbar area start Y - 10px from top divider)
        const float scrollbar_area_height = 646.0f - scrollbar_area_y - 10.0f; // 滑动条区域高度 - 距离下分界线10像素 (Scrollbar area height - 10px from bottom divider)
        
        // 计算滑动条滑块的位置和大小 (Calculate scrollbar thumb position and size)
        const float total_items = static_cast<float>(this->newdialog_list_items.size());
        const float visible_ratio = static_cast<float>(items_per_page) / total_items; // 可见项目比例 (Visible items ratio)
        const float thumb_height = scrollbar_area_height * visible_ratio; // 滑块高度 (Thumb height)
        const float scroll_ratio = static_cast<float>(start_index) / (total_items - items_per_page); // 滚动比例 (Scroll ratio)
        const float thumb_y = scrollbar_area_y + (scrollbar_area_height - thumb_height) * scroll_ratio; // 滑块Y坐标 (Thumb Y position)
        
        // 绘制滑动条滑块 - 只用灰色，不需要背景 (Draw scrollbar thumb - only grey color, no background)
        gfx::drawRect(this->vg, scrollbar_x, thumb_y, scrollbar_width, thumb_height, gfx::Colour::GREY);
        
    }

}

void App::newUpdateDialogListSelect() {

    if (this->newdialog_list_items.empty()) {
        if (this->controller.B) {
            // B键取消选择 (B key to cancel selection)
            this->audio_manager.PlayConfirmSound();
            this->newHideDialog();
        }
        return;
    }

    // 上下键移动选择 (Up/Down keys to move selection)
    if (this->controller.UP) {
        if (this->newdialog_list_selected_index > 0) {
            this->audio_manager.PlayKeySound(0.9);
            this->newdialog_list_selected_index--;
            // 如果选中项超出当前页面范围，调整起始索引 (If selected item is out of current page range, adjust start index)
            if (this->newdialog_list_selected_index < this->newdialog_list_start_index) {
                this->newdialog_list_start_index = this->newdialog_list_selected_index;
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.DOWN) {
        if (this->newdialog_list_selected_index < this->newdialog_list_items.size() - 1) {
            this->audio_manager.PlayKeySound(0.9);
            this->newdialog_list_selected_index++;
            // 如果选中项超出当前页面范围，调整起始索引 (If selected item is out of current page range, adjust start index)
            if (this->newdialog_list_selected_index >= this->newdialog_list_start_index + 6) {
                this->newdialog_list_start_index = this->newdialog_list_selected_index - 5;
            }
        } else {
            this->audio_manager.PlayLimitSound(1.5);
        }
    } else if (this->controller.L) { // L键向上快速滚动 (L key for fast scroll up)
        if (this->newdialog_list_items.size() > 0) {
            const size_t items_per_page = 6;
            
            if (this->newdialog_list_selected_index >= items_per_page) {
                // 向上移动一页的距离 (Move up by one page distance)
                this->audio_manager.PlayKeySound(0.9);
                this->newdialog_list_selected_index -= items_per_page;
                
                // 调整起始索引以确保选中项可见 (Adjust start index to ensure selected item is visible)
                if (this->newdialog_list_selected_index < this->newdialog_list_start_index) {
                    this->newdialog_list_start_index = this->newdialog_list_selected_index;
                }
            } else {
                // 移动到第一个项目 (Move to first item)
                if (this->newdialog_list_selected_index != 0) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->newdialog_list_selected_index = 0;
                    this->newdialog_list_start_index = 0;
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        }
    } else if (this->controller.R) { // R键向下快速滚动 (R key for fast scroll down)
        if (this->newdialog_list_items.size() > 0) {
            const size_t items_per_page = 6;
            
            if (this->newdialog_list_selected_index + items_per_page < this->newdialog_list_items.size()) {
                // 向下移动一页的距离 (Move down by one page distance)
                this->audio_manager.PlayKeySound(0.9);
                this->newdialog_list_selected_index += items_per_page;
                
                // 调整起始索引以确保选中项可见 (Adjust start index to ensure selected item is visible)
                if (this->newdialog_list_selected_index >= this->newdialog_list_start_index + items_per_page) {
                    this->newdialog_list_start_index = this->newdialog_list_selected_index - items_per_page + 1;
                }
            } else {
                // 移动到最后一个项目 (Move to last item)
                const size_t last_index = this->newdialog_list_items.size() - 1;
                if (this->newdialog_list_selected_index != last_index) {
                    this->audio_manager.PlayKeySound(0.9);
                    this->newdialog_list_selected_index = last_index;
                    
                    // 调整起始索引以确保最后一项可见 (Adjust start index to ensure last item is visible)
                    if (last_index >= items_per_page - 1) {
                        this->newdialog_list_start_index = last_index - items_per_page + 1;
                    } else {
                        this->newdialog_list_start_index = 0;
                    }
                } else {
                    this->audio_manager.PlayLimitSound(1.5);
                }
            }
        }
    } else if (this->controller.A) {
        // A键处理：根据对话框类型决定是多选还是单选 (A key handling: decide multi-select or single-select based on dialog type)
        this->audio_manager.PlayConfirmSound();
        
        if (this->multiple_select_mode) {
            // 多选模式：切换当前项的选中状态 (Multi-select mode: toggle current item selection state)
            size_t current_index = this->newdialog_list_selected_index;
            std::string current_item = this->newdialog_list_items[current_index];
            
            // 检查当前项是否已选中 (Check if current item is already selected)
            if (this->newdialog_list_selected_indices.count(current_index)) {
                // 已选中，取消选中 (Already selected, deselect it)
                this->newdialog_list_selected_indices.erase(current_index);
                // 从选中项列表中移除 (Remove from selected items list)
                auto it = std::find(this->newdialog_list_selected_items.begin(), 
                                    this->newdialog_list_selected_items.end(), current_item);
                if (it != this->newdialog_list_selected_items.end()) {
                    this->newdialog_list_selected_items.erase(it);
                }
            } else {
                // 未选中，添加到选中列表 (Not selected, add to selected list)
                this->newdialog_list_selected_indices.insert(current_index);
                this->newdialog_list_selected_items.push_back(current_item);
            }
        } else {
            // 单选模式：选择当前项并调用回调 (Single-select mode: select current item and call callback)
            if (this->newdialog_list_callback) {
                std::vector<std::string> selected_items = {this->newdialog_list_items[this->newdialog_list_selected_index]};
                auto callback = this->newdialog_list_callback; // 保存回调函数 (Save callback function)
                this->newHideDialog(); // 先隐藏对话框 (Hide dialog first)
                callback(selected_items, false); // 然后调用回调 (Then call callback)
            } else {
                this->newHideDialog();
            }
        }
    } else if (this->controller.B) {

        // B键取消选择 (B key to cancel selection)
        this->audio_manager.PlayConfirmSound();
        if (this->newdialog_list_callback) {
            std::vector<std::string> empty_selection;
            auto callback = this->newdialog_list_callback; // 保存回调函数 (Save callback function)
            this->newHideDialog(); // 先隐藏对话框 (Hide dialog first)
            callback(empty_selection, true); // 然后调用回调 (Then call callback)
        } else {
            this->newHideDialog();
        }

    } else if (this->multiple_select_mode && this->controller.START){

        // START键确认多选 (START key to confirm multi-selection)
        this->audio_manager.PlayConfirmSound();
        if (this->newdialog_list_callback) {
            auto callback = this->newdialog_list_callback; // 保存回调函数 (Save callback function)
            auto selected_items = this->newdialog_list_selected_items; // 保存选中项 (Save selected items)
            this->newHideDialog(); // 先隐藏对话框 (Hide dialog first)
            callback(selected_items, false); // 然后调用回调 (Then call callback)
        } else {
            this->newHideDialog();
        }

    }

    
}




void App::newShowDialogListSelect(const std::string& newdialog_list_title,
                                  const std::vector<std::string>& newdialog_list_items,
                                  bool multiple_select_mode,
                                  DialogListSelectCallback callback,
                                  float newdialog_content_font_size) {
    
    this->newdialog_type = newDialogType::LIST_SELECT;                                     
    this->show_newdialog = true;
    this->newdialog_list_title = newdialog_list_title;
    this->newdialog_list_items = newdialog_list_items;
    this->multiple_select_mode = multiple_select_mode;
    this->newdialog_list_callback = callback;
    this->newdialog_content_font_size = newdialog_content_font_size;
    this->newdialog_list_selected_index = 0;
    this->newdialog_list_start_index = 0;
}

void App::newHideDialogListSelect() {

    this->newdialog_list_items.clear();
    this->newdialog_list_selected_indices.clear();
    this->newdialog_list_selected_items.clear();

    this->multiple_select_mode = false;
    this->newdialog_list_callback = nullptr;

}
        
    


// =================================================列表对话框结束====================================================

// =================================================进度对话框====================================================


void App::newDrawDialogCopyProgress() {

    const float dialog_width = 650.0f;
    const float dialog_height = 350.0f;
    // 对话框位置 (Dialog position)
    const float dialog_x = (SCREEN_WIDTH - dialog_width) / 2.0f;
    const float dialog_y = (SCREEN_HEIGHT - dialog_height) / 2.0f;

    // 获取复制进度信息 (Get copy progress info)
    CopyProgressInfo progress_info;
    {
        std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
        progress_info = this->copy_progress;
    }

    // 绘制对话框背景 (Draw dialog background)
    gfx::drawRoundedRect(this->vg, dialog_x, dialog_y, dialog_width, dialog_height, 6.0f, 45, 45, 45);
    
    // 绘制标题栏分界线 (Draw title divider line)
    gfx::drawRect(this->vg, dialog_x, dialog_y + 70.0f, dialog_width, 1.0f, nvgRGB(100, 100, 100));

    // 如果回调里面没有设置新的标题就用默认调用的
    if (!progress_info.dialog_title.empty()) this->newdialog_title = progress_info.dialog_title;
    // 绘制标题 (Draw title)
    gfx::drawTextBoxCentered(this->vg, dialog_x, dialog_y, dialog_width, 70.0f,
                    24.0f, 1.4f,
                    this->newdialog_title.c_str(), nullptr,
                    NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE);
    
    
    // 进度条位置计算 (Calculate progress bar position)
    const float progress_bar_width = dialog_width - 120;
    const float progress_bar_height = 30.0f;
    const float progress_bar_x = dialog_x + (dialog_width - progress_bar_width) / 2;
    const float progress_bar_y = dialog_y + dialog_height * 3 / 4 - progress_bar_height / 2;
    
    // 绘制进度条背景 (Draw progress bar background)
    gfx::drawRoundedRect(this->vg, progress_bar_x, progress_bar_y, progress_bar_width, progress_bar_height, 6.0f, 60, 60, 60);
    
    // 进度条颜色默认是蓝色
    int bar_color_R = progress_info.progress_bar_color[0];
    int bar_color_G = progress_info.progress_bar_color[1];
    int bar_color_B = progress_info.progress_bar_color[2];

    // 绘制进度条填充 (Draw progress bar fill)
    if (progress_info.progress_percentage > 0.0f) {
        const float fill_width = progress_bar_width * (progress_info.progress_percentage / 100.0f);
        gfx::drawRoundedRect(this->vg, progress_bar_x, progress_bar_y, fill_width, progress_bar_height, 6.0f, bar_color_R, bar_color_G, bar_color_B); // 进度条颜色 (Progress bar color)
    }

    gfx::drawTextBoxCentered(this->vg, dialog_x, dialog_y + dialog_height / 4 , dialog_width, dialog_height / 4 + 40.f, 
                            24.0f , 1.4f, 
                            this->newdialog_content.c_str(), nullptr, 
                            NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE);

    // 绘制当前复制的文件名 (Draw current copying file name)
    if (!progress_info.current_file.empty()) {
        gfx::drawTextBoxCentered(this->vg, progress_bar_x +3.f, progress_bar_y-35.0f, progress_bar_width - 6.f, 35.0f ,
                            20.0f, 1.4f,
                            progress_info.current_file.c_str(), nullptr,
                            NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE);
    }

    // 文件数量背景 ，防止被文件名挡住了
    gfx::drawRoundedRect(this->vg, dialog_x + dialog_width * 0.65, progress_bar_y-45.0f, dialog_width * 0.35, 45.0f, 0.0f, 45, 45, 45);

    // 绘制文件进度信息 (Draw file progress info)
    char progress_text[128];
    snprintf(progress_text, sizeof(progress_text), "%zu / %zu",
            progress_info.files_copied, progress_info.total_files);
    gfx::drawTextBoxCentered(this->vg, progress_bar_x + 3.f, progress_bar_y-35.0f, progress_bar_width - 6.f, 35.0f ,
                        20.0f, 1.4f,
                        progress_text, nullptr,
                        NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE);

        
        
    // 绘制进度百分比文字 (Draw progress percentage text)
    snprintf(progress_text, sizeof(progress_text), "%.1f%%", progress_info.progress_percentage);
    gfx::drawText(this->vg, progress_bar_x + progress_bar_width / 2.0f, progress_bar_y + progress_bar_height / 2.0f, 18.0f, progress_text, nullptr, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, gfx::Colour::WHITE);
    
    // 绘制文件级进度条 (Draw file-level progress bar)
    if (progress_info.is_copying_file && progress_info.file_progress_percentage > 0.0f) {
        const float file_progress_bar_y = progress_bar_y + progress_bar_height + 10.0f; // 位于总进度条下方10像素 (10 pixels below total progress bar)
        const float file_progress_bar_height = 3.0f; // 宽度3像素 (3 pixels width)
        const float file_fill_width = progress_bar_width * (progress_info.file_progress_percentage / 100.0f);
        
        // 绘制文件级进度条填充 (Draw file-level progress bar fill)
        gfx::drawRect(this->vg, progress_bar_x, file_progress_bar_y, file_fill_width, file_progress_bar_height, nvgRGB(0, 150, 255)); // 绿色进度条 (Green progress bar)
    }


}





void App::newShowDialogCopyProgress(const std::string& newdialog_title,
                                const std::string& newdialog_content){
    
    
    this->newdialog_type = newDialogType::COPY_PROGRESS;
    this->newdialog_title = newdialog_title;
    this->newdialog_content = newdialog_content;
    this->show_newdialog = true;


}


void App::newUpdateCopyProgress(const CopyProgressInfo& progress){
    // 更新复制进度 (Update copy progress)
    std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
    this->copy_progress = progress;
}

void App::newHideDialogCopyProgress(){

    // 重置进度对话框相关状态
    {
        std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
        this->copy_progress = CopyProgressInfo{}; // 重置进度信息
    }

}

void App::newUpdateDialogCopyProgress(){

    // 允许用户按B键取消任務
    if (this->controller.B) {
        

        // 取消安装和卸载任务
        if (!this->mod_uninstalling && copy_task.valid()) {
            this->audio_manager.PlayCancelSound();
            copy_task.request_stop();
            // 不立即隐藏对话框，让用户能看到清理进度
            // 等异步任务完成后再处理UI切换
            return;
        }

        
        // 取消添加任务
        if (add_task.valid()) {
            add_task.request_stop(); // 请求停止添加任务 (Request to stop add task)
            this->audio_manager.PlayCancelSound();

            std::string files_success = "0";
            std::string files_false = "0";

            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                // 如果有具体的错误信息，优先显示具体错误信息 (If there's specific error message, prioritize showing it)
                files_success = std::to_string(this->copy_progress.files_copied);
                files_false = std::to_string(this->copy_progress.total_files - this->copy_progress.files_copied);
            }
            
            // std::string result_text = ADD_MOD_STOP_TEXT + files_appended + " / " + total_files + " MOD";
            std::string duration_str = FormatDuration(this->operation_duration.count() / 1000);
            std::string result_text = GetSnprintf(ADD_MOD_DONE_TEXT, files_success, files_false, duration_str); 
            this->newHideDialog();
            newShowDialogConfirm(result_text);
            return;
        }
            
    }


    // ================== 处理异步安装卸载的任务 ==================
    if (copy_task.valid() &&
        copy_task.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        
        // 处理异步任务结果
        bool task_result = copy_task.get();
        
        // 获取MOD的名字，还有安装状态
        std::string MOD_NAME = this->mod_info[this->mod_index].MOD_NAME2;
        bool MOD_STATE = this->mod_info[this->mod_index].MOD_STATE;

        // 检查是否是因为用户中断而结束的任务
        if (copy_task.get_token().stop_requested()) {
            // 这是中断的情况，现在才隐藏进度对话框并显示确认对话框
            this->newHideDialog();
            this->audio_manager.PlayCancelSound();

            if (MOD_STATE || this->clean_button) {
                newShowDialogConfirm(GetSnprintf(CANCEL_UNINSTALLED, MOD_NAME));
            } else {
                newShowDialogConfirm(GetSnprintf(CANCEL_INSTALLED, MOD_NAME));
            }
            
            this->clean_button = false;
            return;
        }

        // 异步任务结束后运行
        if (task_result) {

            this->audio_manager.PlayConfirmSound();
            this->mod_uninstalling = false;
            // 任务处理时间
            std::string duration_str = FormatDuration(this->operation_duration.count() / 1000);

            // 根据MOD状态和是否清理按钮，显示不同的提示 
            // 如果是按Y键强制卸载到这里，此时的clean_button应当为false
            if (MOD_STATE || this->clean_button) {
                newShowDialogConfirm(GetSnprintf(SUCCESS_UNINSTALLED, MOD_NAME, duration_str));
            } else {
                newShowDialogConfirm(GetSnprintf(SUCCESS_INSTALLED, MOD_NAME, duration_str));
            }

            // 根据MOD状态和是否清理按钮，更新MOD信息
            if (MOD_STATE && this->clean_button) {
                
                this->ChangeModName();
            } else if (!this->clean_button) {
                this->ChangeModName();
            }
            // 重置清理按钮状态
            this->clean_button = false;

            

        } else {

            this->mod_uninstalling = false;
            // 处理异步任务失败情况，error_msg是报错信息，如果没有具体报错，就用默认报错信息。
            std::string error_msg;

            
            if (MOD_STATE || this->clean_button) {
                error_msg = GetSnprintf(FAILURE_UNINSTALLED, MOD_NAME);
                this->clean_button = false;
            } else {
                error_msg = GetSnprintf(FAILURE_INSTALLED, MOD_NAME);
            }

            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                if (!this->copy_progress.error_message.empty()) {
                    error_msg = this->copy_progress.error_message;
                }
            }

            this->audio_manager.PlayCancelSound();
            
            this->newHideDialog();
            newShowDialogConfirm(error_msg);
            
        }
        
        return;
    }
    // ================== 处理异步安装卸载的任务结束 ==================

    // ================== 处理异步添加MOD和游戏的任务 ==================

    if (add_task.valid() &&
        add_task.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            
        bool task_result = add_task.get();
        std::string files_success = "0";
        std::string files_false = "0";

        {
            std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
            files_success = std::to_string(this->copy_progress.files_copied);
            files_false = std::to_string(this->copy_progress.total_files - this->copy_progress.files_copied);
        }

        if (task_result) {

            this->audio_manager.PlayConfirmSound();
            std::string duration_str = FormatDuration(this->operation_duration.count() / 1000);
            std::string result_text = GetSnprintf(ADD_MOD_DONE_TEXT, files_success, files_false, duration_str); 
            this->audio_manager.PlayConfirmSound();
            newShowDialogConfirm(result_text);

        } else {
            std::string error_msg;
            {
                std::lock_guard<std::mutex> lock(this->copy_progress_mutex);
                error_msg = this->copy_progress.error_message.empty()
                            ? UNKNOWN_ERROR
                            : this->copy_progress.error_message;
            }

            this->audio_manager.PlayCancelSound();
            
            this->newHideDialog();
            newShowDialogConfirm(error_msg);
        }

        return;
        
    }

    // ================== 处理异步添加MOD和游戏的任务结束 ==================
    
}

  
// =================================================进度对话框结束====================================================






void App::newListMenuCenter(const std::string& function) {


    if (this->menu_mode == MenuMode::LIST) {

        if (function == LIST_DIALOG_CUSTOM_NAME) {              // 修改游戏名

            this->audio_manager.PlayConfirmSound(1.0);
            changegamename();

        } else if (function == LIST_DIALOG_MODVERSION) {        // 修改游戏版本

            this->audio_manager.PlayConfirmSound(1.0);
            changemodversion();

        } else if (function == LIST_DIALOG_ADDGAME) {            // 添加游戏

            
            if (finished_scanning) {
                this->audio_manager.PlayConfirmSound(1.0);
                this->menu_mode = MenuMode::ADDGAMELIST;
            } else {
                this->audio_manager.PlayCancelSound();
                newShowDialogConfirm(WAIT_SCAN_TEXT);
            }

        } else if (function == LIST_DIALOG_ABOUTAUTHOR) {        // 关于作者

            this->audio_manager.PlayConfirmSound(1.0);

            #define STRINGIZE(x) #x 
            #define STRINGIZE_VALUE_OF(x) STRINGIZE(x) 

            std::string version = STRINGIZE_VALUE_OF(UNTITLED_VERSION_STRING);
            
            newShowDialogConfirm(("作者：TOM\n版本：" + version + "\nQ群：1051287661\n任何BUG欢迎来群反馈"),nullptr,
                        26.0f,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);
    
            #undef STRINGIZE
            #undef STRINGIZE_VALUE_OF
            
            
        } else if (function == LIST_DIALOG_ViewDetails) {        // 查看位置

            this->audio_manager.PlayConfirmSound(1.0);
            
            // 使用读锁保护entries访问 (Use read lock to protect entries access)
            std::string file_path;
            std::string game_name;
            {
                std::scoped_lock lock{entries_mutex};
                file_path = this->entries[this->index].FILE_PATH;
                game_name = this->entries[this->index].FILE_NAME2;
            }
            newShowDialogConfirm(game_name + "\n\n" + file_path,nullptr,
                                    26.0f,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);

        } else if (function == IS_FAVORITE) {                     // 标记喜欢

            tofavorite();

        } else if (function == IS_NOT_FAVORITE) {                     // 取消标记

            notfavorite();

        } else if (function == LIST_DIALOG_REMOVE_GAME) {                     // 移除游戏

            // 创建回调函数来处理用户确认后的操作 (Create callback function to handle operations after user confirmation)
            auto callback = [this](bool confirmed) {
                if (confirmed) {
                    RemoveGame();
                } 
            };

            newShowDialogConfirm(CONFIRM_REMOVE_GAME,callback);

        } else if (function == LIST_DIALOG_MTP) {                     // MTP
            
            if (finished_scanning) {

                // 初始化MTP管理器 (Initialize MTP manager)
                if (!this->mtp_manager) {
                    this->mtp_manager = &mtp::MtpManager::GetInstance();
                }
                this->menu_mode = MenuMode::MTP;

            } else {

                this->audio_manager.PlayCancelSound();
                newShowDialogConfirm(WAIT_SCAN_TEXT);
            
            }



        }

        return;
    }


    if (this->menu_mode == MenuMode::MODLIST) {

        if (function == LIST_DIALOG_CUSTOM_NAME) {              // 修改模组名
            
            this->audio_manager.PlayConfirmSound(1.0);
            changemodname();

        } else if (function == LIST_DIALOG_MODTYPE) {        // 修改模组类型

            
            this->audio_manager.PlayConfirmSound(1.0); // 播放确认音效 (Play confirm sound effect)
        
            std::vector<std::string> option_text = {
                LIST_DIALOG_FPSPATCH,
                LIST_DIALOG_HDPATCH,
                LIST_DIALOG_COSMETICPATCH,
                LIST_DIALOG_PLAYPATCH,
                LIST_DIALOG_CHEATPATCH,
            };

            // 创建回调函数来处理选择结果 (Create callback function to handle selection result)
            auto callback = [this](const std::vector<std::string>& selected_items, bool cancelled) {
                if (!cancelled) {
                    this->Dialog_MODLIST_TYPE_Menu(selected_items[0]);
                }
            };

            newShowDialogListSelect(OPTION_MODTYPE_MEMU_TITLE, option_text,false, callback);


        } else if (function == LIST_DIALOG_MOD_DESCRIPTION) {        // 修改模组描述

            this->audio_manager.PlayConfirmSound(1.0);
            changemoddescription();

        } else if (function == LIST_DIALOG_APPENDMOD) {            // 追加模组

            this->audio_manager.PlayConfirmSound(1.0);

            // 创建回调函数来处理选择结果 (Create callback function to handle selection result)
            auto callback = [this](const std::vector<std::string>& selected_items, bool cancelled) {
                if (!cancelled) {
                    this->Dialog_APPENDMOD_Menu(selected_items);
                }
            };

            newShowDialogListSelect(OPTION_APPENDMOD_MEMU_TITLE, appendmodscan(),true, callback);

        } else if (function == LIST_DIALOG_ViewDetails) {        // 查看模组位置

            this->audio_manager.PlayConfirmSound(1.0);

            std::string mod_path = this->mod_info[this->mod_index].MOD_PATH;
            std::string mod_name = this->mod_info[this->mod_index].MOD_NAME2;

            newShowDialogConfirm(mod_name + "\n\n" + mod_path,nullptr,
                                    26.0f,NVG_ALIGN_LEFT|NVG_ALIGN_MIDDLE);

        } else if (function == LIST_DIALOG_REMOVE_MOD) {        // 移除模组

            this->audio_manager.PlayConfirmSound(1.0);

            // 创建回调函数来处理用户确认后的操作 (Create callback function to handle operations after user confirmation)
            auto callback = [this](bool confirmed) {
                if (confirmed) {
                    RemoveMod();
                } 
            };

            newShowDialogConfirm(CONFIRM_REMOVE_MOD,callback);
            
        }
        return;
    }



}






} // namespace tj
