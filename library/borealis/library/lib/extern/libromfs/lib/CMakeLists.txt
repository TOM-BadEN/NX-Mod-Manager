project("libromfs-${LIBROMFS_PROJECT_NAME}")

set(CMAKE_CXX_STANDARD 20)

# Gather romfs files
file(GLOB_RECURSE ROMFS_FILES
    "${LIBROMFS_RESOURCE_LOCATION}/*"
)

# Add sources
add_library(${PROJECT_NAME} STATIC
    ${ROMFS}
    source/romfs.cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC include)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
target_compile_definitions(${PROJECT_NAME} PUBLIC LIBROMFS_PROJECT_NAME=${LIBROMFS_PROJECT_NAME})

if (USE_BOOST_FILESYSTEM)
    find_package(Boost 1.44 REQUIRED COMPONENTS filesystem)
    if(Boost_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE Boost::filesystem)
        target_compile_options(${PROJECT_NAME} PRIVATE -DUSE_BOOST_FILESYSTEM)
    endif()
endif ()

# Make sure libromfs gets rebuilt when any of the resources are changed
if (LIBROMFS_PREBUILT_GENERATOR)
    message(STATUS "Using prebuilt libromfs-generator: ${LIBROMFS_PREBUILT_GENERATOR}")
    add_custom_command(OUTPUT ${ROMFS}
            COMMAND ${LIBROMFS_PREBUILT_GENERATOR}
                ${LIBROMFS_PROJECT_NAME} ${LIBROMFS_RESOURCE_LOCATION}
            DEPENDS ${ROMFS_FILES}
            )
else ()
    message(STATUS "Using libromfs-generator: $<TARGET_FILE:libromfs-generator>")
    add_custom_command(OUTPUT ${ROMFS}
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} $<TARGET_FILE:libromfs-generator>
                ${LIBROMFS_PROJECT_NAME} ${LIBROMFS_RESOURCE_LOCATION}
            DEPENDS ../generator ${ROMFS_FILES}
            )
endif ()

add_custom_target(romfs_file_packer-${LIBROMFS_PROJECT_NAME} ALL DEPENDS ${ROMFS_FILES})