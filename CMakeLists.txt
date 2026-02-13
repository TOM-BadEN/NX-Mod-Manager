# ============================================================================
# 基础配置
# ============================================================================

# 要求 CMake 最低版本 3.10
cmake_minimum_required(VERSION 3.10)  

# Borealis 库路径
set(BOREALIS_LIBRARY ${CMAKE_CURRENT_SOURCE_DIR}/library/borealis/library)

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
set(VERSION_MINOR "0")                       # 次版本号
set(VERSION_ALTER "0")                       # 修订版本号
set(VERSION_BUILD "0")                       # 构建号
set(PROJECT_AUTHOR "TOM")               # 作者名 (显示在 Switch 主菜单)
set(PROJECT_ICON ${CMAKE_CURRENT_SOURCE_DIR}/resources/img/icon.jpg)  # Switch 图标 (256x256 JPG)
set(PROJECT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources)               # 资源目录

# GCC 编译选项
add_compile_options(-fdiagnostics-color=always -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft)

# 添加 Borealis 库子目录
add_subdirectory(library)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/library/libnxtc-add-version/include
    ${CMAKE_CURRENT_SOURCE_DIR}/library/libnxtc-add-version/source
)
file(GLOB_RECURSE LIB_SRC library/yyjson/*.c library/libnxtc-add-version/source/*.c)
list(APPEND MAIN_SRC ${LIB_SRC})

# ============================================================================
# Switch 平台配置
# ============================================================================
# Switch 需要链接的系统库
set(APP_PLATFORM_LIB
    glfw3 EGL glapi drm_nouveau  # 图形库
    nx m                          # Nintendo Switch SDK
)
# Switch 平台封装代码
list(APPEND MAIN_SRC ${BOREALIS_LIBRARY}/lib/platforms/switch/switch_wrapper.c)


# ============================================================================
# 构建目标
# ============================================================================
program_target(${PROJECT_NAME} "${MAIN_SRC}")
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)  # 使用 C++17 标准

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
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/resources/font
    COMMAND ${NX_ELF2NRO_EXE} ${PROJECT_NAME}.elf ${PROJECT_NAME}.nro --icon=${PROJECT_ICON} --nacp=${PROJECT_NAME}.nacp --romfsdir=${CMAKE_BINARY_DIR}/resources
)

# ============================================================================
# 最终链接配置 (必须保留)
# ============================================================================
target_include_directories(${PROJECT_NAME} PRIVATE ${APP_PLATFORM_INCLUDE})
target_compile_options(${PROJECT_NAME} PRIVATE -ffunction-sections -fdata-sections)
target_link_libraries(${PROJECT_NAME} PRIVATE borealis ${APP_PLATFORM_LIB})
