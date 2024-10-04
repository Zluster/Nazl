cmake_minimum_required(VERSION 3.10)

project(TestProject)
file(GLOB CPP_FILES "*.cpp")

foreach (CPP_FILE ${CPP_FILES})
    get_filename_component(CPP_FILE_NAME ${CPP_FILE} NAME_WE)

    add_executable(${CPP_FILE_NAME} ${CPP_FILE})

    target_link_libraries(${CPP_FILE_NAME} PRIVATE common)

    target_include_directories(${CPP_FILE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/common)

    install(TARGETS ${CPP_FILE_NAME}
            LIBRARY DESTINATION lib
            RUNTIME DESTINATION bin
    )
endforeach ()