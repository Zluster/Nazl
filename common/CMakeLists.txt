cmake_minimum_required(VERSION 3.10)

find_package(Threads REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(fmt REQUIRED)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(common SHARED
        config.cpp
        timer.cpp
        file_ops.cpp
        log/log.cpp
        pool/thread_pool.cpp
)

target_link_libraries(common PUBLIC Threads::Threads yaml-cpp fmt::fmt)
target_include_directories(common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/log
        ${CMAKE_CURRENT_SOURCE_DIR}/pool)
install(TARGETS common
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin
)
install(FILES config.h file_ops.h log/log.h pool/thread_pool.h DESTINATION include)