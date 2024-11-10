# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(UsbLib)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)

include_directories(${LIBUSB_INCLUDE_DIRS})

add_library(usblib
    usbDevice.cpp
    usbTransfer.cpp

)

target_link_libraries(usblib
        ${LIBUSB_LIBRARIES}
        pthread
)
