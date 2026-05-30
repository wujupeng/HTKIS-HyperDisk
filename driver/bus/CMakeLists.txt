add_library(hyperdisk_bus STATIC
    bus_driver.c
    bus_pnp.c
    bus_ioctl.c
    bus_child.c
)

target_link_libraries(hyperdisk_bus PRIVATE hd_driver_common)
