# ===========================================================================
# reverse_algo_lib CMakeLists.txt
#
# 构建 stereo_algo 静态库和验证测试程序
#
# 用法 (独立构建):
#   mkdir build && cd build
#   cmake .. && cmake --build .
#   ./test_with_dump_data ../../dump_output
#
# 用法 (被上层 CMake 引用):
#   add_subdirectory(reverse_algo_lib)
#   target_link_libraries(my_target PRIVATE stereo_algo)
# ===========================================================================

cmake_minimum_required(VERSION 3.10)
project(stereo_algo_lib CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===========================================================================
# 静态库: stereo_algo (核心算法, 无 SDK 依赖)
# ===========================================================================

add_library(stereo_algo STATIC
    StereoAlgoLib.cpp
    StereoAlgoLib.h
)

target_include_directories(stereo_algo PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ===========================================================================
# 静态库: stereo_algo_pipeline (SDK 兼容封装, 依赖 SDK 头文件)
#
# 仅当 SDK include 路径可用时才构建
# ===========================================================================

# 自动检测 SDK include 路径
if(NOT FTK_INCLUDE_DIR)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../fusionTrack_SDK-v4.10.1-linux64/include")
        set(FTK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../fusionTrack_SDK-v4.10.1-linux64/include")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../fusionTrack SDK x64/include")
        set(FTK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../fusionTrack SDK x64/include")
    endif()
endif()

if(FTK_INCLUDE_DIR)
    add_library(stereo_algo_pipeline STATIC
        StereoAlgoPipeline.cpp
        StereoAlgoPipeline.h
    )

    target_include_directories(stereo_algo_pipeline PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${FTK_INCLUDE_DIR}
    )

    target_link_libraries(stereo_algo_pipeline PUBLIC stereo_algo)
    message(STATUS "stereo_algo_pipeline: SDK include found at ${FTK_INCLUDE_DIR}")
else()
    message(STATUS "stereo_algo_pipeline: SDK include not found, pipeline not built")
endif()

# ===========================================================================
# 验证测试程序: test_with_dump_data (仅使用核心库, 无 SDK 依赖)
# ===========================================================================

add_executable(test_with_dump_data
    test_with_dump_data.cpp
)

target_link_libraries(test_with_dump_data PRIVATE stereo_algo)
