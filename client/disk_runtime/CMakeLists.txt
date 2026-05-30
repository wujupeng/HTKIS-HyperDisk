add_library(hd_disk_runtime STATIC
    disk_runtime.cpp
    virtual_disk.cpp
    block_io.cpp
    network_state_machine.cpp
)

target_link_libraries(hd_disk_runtime PRIVATE hd_client_common)
