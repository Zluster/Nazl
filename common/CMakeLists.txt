cmake_minimum_required(VERSION 3.10)

# 项目名称和版本
project(Nazl VERSION 1.0)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_compile_options(-g)
# 查找线程库
find_package(Threads REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(fmt REQUIRED)

# 包含头文件目录
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/log)
include_directories(${PROJECT_SOURCE_DIR}/pool)
# 添加可执行文件，显式列出所有源文件
add_executable(test_log
        example/test_log.cpp
        config.cpp
        file_ops.cpp
        log/log.cpp
        pool/thread_pool.cpp
        # 确保此处列出的为实际需要的所有源文件
        # 例如, 另需 source1.cpp source2.cpp
)

# 链接需要的库
target_link_libraries(test_log PRIVATE Threads::Threads yaml-cpp fmt::fmt)
