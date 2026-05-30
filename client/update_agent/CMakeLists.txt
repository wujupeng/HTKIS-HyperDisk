add_library(hd_update_agent STATIC
    update_agent.cpp
    delta_sync.cpp
    version_mgr.cpp
)

target_link_libraries(hd_update_agent PRIVATE hd_client_common)
