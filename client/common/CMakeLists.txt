add_library(hd_client_common STATIC
    hd_config.cpp
    hd_log.cpp
    hd_ipc.cpp
    hd_util.cpp
    hd_boot_trace.cpp
    hd_frame_codec.cpp
)

target_include_directories(hd_client_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
