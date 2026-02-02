cmake_minimum_required(VERSION 3.16)
project(libromfs-generator)
set(CMAKE_CXX_STANDARD 20)

# Add sources
add_executable(${PROJECT_NAME}
    source/main.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE include)

if (USE_BOOST_FILESYSTEM)
    find_package(Boost 1.44 REQUIRED COMPONENTS filesystem)
    if(Boost_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE Boost::filesystem)
        target_compile_options(${PROJECT_NAME} PRIVATE -DUSE_BOOST_FILESYSTEM)
    endif()
endif ()