cmake_minimum_required(VERSION 3.10)
project(base)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 检查编译器是否支持 C++17
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++17" COMPILER_SUPPORTS_CXX17)
if(NOT COMPILER_SUPPORTS_CXX17)
    message(FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++17 support.")
endif()

# 设置源文件列表，包含 .h 和 .cc 文件
set(SOURCE_FILES
    base/simple_thread_safe_queue.h
    base/simple_thread_safe_queue.cc
    base/locked_queue.h
)

# 检查源文件是否存在
foreach(file ${SOURCE_FILES})
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${file})
        message(FATAL_ERROR "Source file ${file} does not exist!")
    endif()
endforeach()

# 添加库的源文件，将 base 设为静态库
add_library(base STATIC ${SOURCE_FILES})

# 没有设置单独的 include 目录，指定根目录为包含目录
target_include_directories(base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 添加 gtest 子模块目录
add_subdirectory(third_party/gtest)

# 启用测试功能
enable_testing()

# 添加测试可执行文件
add_executable(BaseTest
    test/base_main.cc
    test/thread_safe_queue_test.cc
)

# 链接 gtest 和 gtest_main 库到测试可执行文件
target_link_libraries(BaseTest
    PRIVATE
    base
    gtest
    gtest_main
)

# 添加测试用例
add_test(
    NAME BaseTest
    COMMAND BaseTest
) 