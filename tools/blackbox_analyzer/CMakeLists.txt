cmake_minimum_required(VERSION 3.24)
project(boot_blackbox_analyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(boot_blackbox_analyzer
    main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../client/blackbox/blackbox_recorder.cpp
)

target_include_directories(boot_blackbox_analyzer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../client/blackbox
)

if(MSVC)
    target_compile_definitions(boot_blackbox_analyzer PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_compile_options(boot_blackbox_analyzer PRIVATE /W4)
endif()
