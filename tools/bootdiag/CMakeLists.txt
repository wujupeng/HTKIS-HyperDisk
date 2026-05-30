cmake_minimum_required(VERSION 3.24)
project(bootdiag LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    add_compile_options(/W4 /wd4100)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

add_executable(bootdiag
    main.c
    bootdiag_checks.c
)

if(WIN32)
    target_link_libraries(bootdiag PRIVATE ws2_32)
endif()

if(MSVC)
    set_target_properties(bootdiag PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_custom_command(TARGET bootdiag POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:bootdiag>"
            "${CMAKE_BINARY_DIR}/bin/bootdiag.exe"
        COMMENT "Copying bootdiag.exe to bin directory"
    )
endif()
