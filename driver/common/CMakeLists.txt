add_library(hd_driver_common STATIC
    hd_debug.c
    hd_memory.c
    hd_lock.c
    hd_registry.c
    hd_serial.c
)

target_include_directories(hd_driver_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
