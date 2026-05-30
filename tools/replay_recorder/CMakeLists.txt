cmake_minimum_required(VERSION 3.24)
project(replay_recorder LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
    add_compile_options(/W4 /wd4100)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

add_library(replay_recorder_lib STATIC
    replay_recorder.cpp
)

target_include_directories(replay_recorder_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(boot_replay_analyzer
    replay_analyzer_main.cpp
)

target_link_libraries(boot_replay_analyzer PRIVATE replay_recorder_lib)

if(MSVC)
    set_target_properties(boot_replay_analyzer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()
