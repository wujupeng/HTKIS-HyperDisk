add_library(hd_driver_manager STATIC
    driver_manager.cpp
    hw_info_collector.cpp
    driver_matcher.cpp
)

target_link_libraries(hd_driver_manager PRIVATE hd_client_common)
