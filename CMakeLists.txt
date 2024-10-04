cmake_minimum_required(VERSION 3.10)

project(Nazl VERSION 1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/output" CACHE PATH "Install path prefix" FORCE)

add_compile_options(-g)

set(SUBPROJECTS
        common
        config
        hal
        app
        test
)


foreach(SUBPROJECT ${SUBPROJECTS})
    add_subdirectory(${SUBPROJECT})
endforeach()