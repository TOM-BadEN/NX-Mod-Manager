# ============================================================================
# 基础配置
# ============================================================================

# CMake 4.0+ 兼容：允许子项目使用低版本 policy
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "")
# 要求 CMake 最低版本 3.10
cmake_minimum_required(VERSION 3.10)
# 兼容高版本 CMake，避免旧代码 policy 报错
cmake_policy(VERSION 3.5)  

# Borealis 库路径
set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/library)

# Switch 平台固定配置（纯 Switch 项目，无需从命令行传入）
set(PLATFORM_SWITCH ON CACHE BOOL "" FORCE)
set(USE_DEKO3D ON CACHE BOOL "" FORCE)

# 引入 Borealis 通用构建选项
include(${BOREALIS_LIBRARY}/cmake/commonOption.cmake)

# ============================================================================
# 依赖库配置 (一般不需要修改)
# ============================================================================
option(USE_SHARED_LIB "Whether to use shared libs provided by system" OFF)
cmake_dependent_option(USE_SYSTEM_FMT "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TINYXML2 "" OFF "NOT USE_SHARED_LIB" ON)
cmake_dependent_option(USE_SYSTEM_TWEENY "" OFF "NOT USE_SHARED_LIB" ON)

# 引入工具链配置 (必须保留)
include(${BOREALIS_LIBRARY}/cmake/toolchain.cmake)

# ============================================================================
# 项目信息 (★ 需要修改成你自己的 ★)
# ============================================================================
project(NX-Mod-Manager)  # 项目名称
set(VERSION_MAJOR "3")                       # 主版本号
set(VERSION_MINOR "1")                       # 次版本号
set(VERSION_ALTER "5")                       # 修订版本号
set(VERSION_BUILD "0")                       # 构建号
set(PROJECT_AUTHOR "TOM")               # 作者名 (显示在 Switch 主菜单)
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/icon.jpg)  # Switch 图标 (256x256 JPG)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)               # 资源目录

# GCC 编译选项
add_compile_options(-fdiagnostics-color=always -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft)

# 添加 Borealis 库子目录
add_subdirectory(library)

# 修复: 页面滑动动画时双 Activity 同时渲染导致 deko3d 命令缓冲区溢出崩溃
# 原因: dk_renderer.hpp 中 DynamicCmdSize 默认 0x20000(128KB)，两个页面同时绘制时 GPU 命令超出此限制
# 方案: 在 cmake configure 阶段自动从框架原文件读取头文件，替换缓冲区大小后写入 build 目录
#       利用 BEFORE 关键字使 build 目录的 include 路径优先于框架原路径，编译器优先找到补丁版本
#       这样无需修改框架子模块源码，且框架更新后重新 configure 会自动基于最新源文件重新生成补丁
# 步骤1: 读取框架原始头文件内容
file(READ "${BOREALIS_LIBRARY}/include/borealis/extern/nanovg/dk_renderer.hpp" _DK_HPP)
# 步骤2: 将缓冲区大小从 128KB 替换为 256KB
string(REPLACE "DynamicCmdSize = 0x20000" "DynamicCmdSize = 0x40000" _DK_HPP "${_DK_HPP}")
# 校验: 确认替换成功，若框架更新了原始值则终止构建
string(FIND "${_DK_HPP}" "DynamicCmdSize = 0x40000" _DK_MATCH_POS)
if (_DK_MATCH_POS EQUAL -1) 
    message(FATAL_ERROR "DkRenderer DynamicCmdSize 补丁失败，框架头文件可能已变更，请检查 dk_renderer.hpp")
endif()
# 步骤3: 将补丁后的头文件写入 build 目录 (不进 git)
file(WRITE "${CMAKE_BINARY_DIR}/override_includes/nanovg/dk_renderer.hpp" "${_DK_HPP}")
# 步骤4: 将 build 目录的 include 路径插入到 borealis 目标的最前面，使编译器优先使用补丁版本
target_include_directories(borealis BEFORE PUBLIC ${CMAKE_BINARY_DIR}/override_includes)

# ============================================================================
# 项目源文件
# ============================================================================
list(APPEND APP_PLATFORM_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/code/include)
file(GLOB_RECURSE MAIN_SRC code/*.cpp)

# ============================================================================
# 第三方库源文件
# ============================================================================
list(APPEND APP_PLATFORM_INCLUDE
    ${CMAKE_CURRENT_SOURCE_DIR}/library/yyjson
    ${CMAKE_CURRENT_SOURCE_DIR}/library/libnxtc-mod/include
    ${CMAKE_CURRENT_SOURCE_DIR}/library/libnxtc-mod/source
    ${CMAKE_CURRENT_SOURCE_DIR}/library/cpp-pinyin/include       # 拼音库
    ${CMAKE_CURRENT_SOURCE_DIR}/library/cpp-pinyin/src
    ${CMAKE_CURRENT_SOURCE_DIR}/library/cpp-pinyin/src/toneUtil
    ${CMAKE_CURRENT_SOURCE_DIR}/library/lunasvg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/library/QR-Code-generator/cpp
)
file(GLOB_RECURSE LIB_SRC library/yyjson/*.c library/libnxtc-mod/source/*.c library/cpp-pinyin/src/*.cpp library/QR-Code-generator/cpp/qrcodegen.cpp)
list(APPEND MAIN_SRC ${LIB_SRC})

# ============================================================================
# Switch 平台配置
# ============================================================================
# Switch 需要链接的系统库
set(APP_PLATFORM_LIB
    curl z                        # HTTP 请求 + zlib 压缩
    glfw3 EGL glapi drm_nouveau  # 图形库
    nx m                          # Nintendo Switch SDK
    webp                          # WebP 图片解码
)
# Switch 平台封装代码
list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)


# ============================================================================
# 构建目标
# ============================================================================
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)  # 使用 C++20 标准

# nxlink 调试开关（make debug 时启用）
if (NXLINK)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NXLINK)
endif()

# 版本号传递给 C++ 代码（ActionMenu Header 显示）
target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}")

# ============================================================================
# 构建 Switch .nro 文件
# ============================================================================
# 如果使用 deko3d 渲染器，生成着色器
if (USE_DEKO3D)
    gen_dksh("${PROJECT_RESOURCES}/shaders")
endif ()

# 构建 .nro 文件的步骤:
# 1. 创建 .nacp 元数据文件 (名称、作者、版本)
# 2. 复制资源目录
# 3. 删除字体目录 (Switch 使用系统字体)
# 4. 打包成 .nro (包含图标、元数据、资源)
add_custom_target(${PROJECT_NAME}.nro DEPENDS ${PROJECT_NAME}
    COMMAND ${NX_NACPTOOL_EXE} --create "${PROJECT_NAME}" "${PROJECT_AUTHOR}" "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_ALTER}" ${PROJECT_NAME}.nacp --titleid=${PROJECT_TITLEID}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_RESOURCES} ${CMAKE_BINARY_DIR}/resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/library/cpp-pinyin/res/dict/mandarin ${CMAKE_BINARY_DIR}/resources/dict/mandarin
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/font
    COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
)

# ============================================================================
# 最终链接配置 (必须保留)
# ============================================================================
target_include_directories(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections)
target_link_libraries(${PROJECT_NAME} PRIVATE borealis libhaze libftpsrv lunasvg miniz ${APP_PLATFORM_LIB})
