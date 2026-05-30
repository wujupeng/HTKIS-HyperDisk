cmake_minimum_required(VERSION 3.24)
project(HTKIS-HyperDisk VERSION 0.1.0 LANGUAGES C CXX)

message(STATUS "=== HTKIS HyperDisk X Build System ===")
message(STATUS "Version: ${PROJECT_VERSION}")

option(BUILD_DRIVER "Build Windows kernel drivers" OFF)
option(BUILD_CLIENT "Build client runtime" ON)
option(BUILD_SERVER "Build server components" ON)
option(BUILD_TOOLS "Build tools (bootdiag, replay_analyzer)" ON)
option(BUILD_TESTS "Build tests" ON)

if(BUILD_DRIVER)
    message(STATUS "Building: Windows Kernel Drivers")
    add_subdirectory(driver)
endif()

if(BUILD_CLIENT)
    message(STATUS "Building: Client Runtime")
    add_subdirectory(client)
endif()

if(BUILD_SERVER)
    message(STATUS "Building: Server Components (C++)")
    add_subdirectory(server)
endif()

if(BUILD_TOOLS)
    message(STATUS "Building: Tools")
    add_subdirectory(tools)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
