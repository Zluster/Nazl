cmake_minimum_required(VERSION 3.10)
project(camera)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)

add_library(camera
    isp.cpp
    camera.cpp
)

target_link_libraries(camera
        ${LIBUSB_LIBRARIES}
        pthread
)
