add_library(hd_overlay_mgr STATIC
    overlay_mgr.cpp
    ram_overlay.cpp
    nvme_overlay.cpp
    journal_wal.cpp
)

target_link_libraries(hd_overlay_mgr PRIVATE hd_client_common)
