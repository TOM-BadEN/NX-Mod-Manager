add_subdirectory(borealis/library)
add_subdirectory(libhaze)
add_subdirectory(libftpsrv)
add_subdirectory(lunasvg)

add_subdirectory(miniz)

# lunasvg v2.3.5 未显式 include 整数类型头文件，GCC 15 要求显式包含
target_compile_options(lunasvg PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-include stdint.h>
    $<$<COMPILE_LANGUAGE:CXX>:-include cstdint>
)