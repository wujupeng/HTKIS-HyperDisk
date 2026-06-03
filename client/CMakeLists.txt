cmake_minimum_required(VERSION 3.24)
project(HyperDiskClient LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(
    _WIN32_WINNT=0x0A00
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

add_compile_options(/wd4100)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

add_subdirectory(common)
add_subdirectory(boot_agent)
add_subdirectory(disk_runtime)
add_subdirectory(smart_cache)
add_subdirectory(overlay_mgr)
add_subdirectory(driver_manager)
add_subdirectory(update_agent)
add_subdirectory(qos_engine)
