add_library(hyperdisk_net STATIC
    net_driver.c
    net_wsk.c
    net_frame.c
    net_connect.c
    net_heartbeat.c
)

target_link_libraries(hyperdisk_net PRIVATE hd_driver_common)
