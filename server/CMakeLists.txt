cmake_minimum_required(VERSION 3.24)
project(HyperDiskServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS system program_options filesystem)
find_package(Threads REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(image_server)
add_subdirectory(cache_engine)
add_subdirectory(dpdk_gateway)
add_subdirectory(ai_engine)
