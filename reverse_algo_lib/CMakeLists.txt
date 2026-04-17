# ===========================================================================
# reverse_algo_lib CMakeLists.txt
#
# 构建 stereo_algo 静态库和验证测试程序
#
# 用法:
#   mkdir build && cd build
#   cmake .. && cmake --build .
#   ./test_with_dump_data ../../dump_output
# ===========================================================================

cmake_minimum_required(VERSION 3.10)
project(stereo_algo_lib CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===========================================================================
# 静态库: stereo_algo
# ===========================================================================

add_library(stereo_algo STATIC
    StereoAlgoLib.cpp
    StereoAlgoLib.h
)

target_include_directories(stereo_algo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ===========================================================================
# 验证测试程序: test_with_dump_data
# ===========================================================================

add_executable(test_with_dump_data
    test_with_dump_data.cpp
)

target_link_libraries(test_with_dump_data PRIVATE stereo_algo)
