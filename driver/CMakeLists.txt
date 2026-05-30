cmake_minimum_required(VERSION 3.24)
project(HyperDiskDrivers LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_definitions(
    _WIN32
    _AMD64_
    _KERNEL_MODE
    NTDDI_VERSION=NTDDI_WIN10_CO
    WINVER=0x0A00
    _WIN32_WINNT=0x0A00
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common
)

add_subdirectory(common)
add_subdirectory(bus)
add_subdirectory(blk)
add_subdirectory(net)
