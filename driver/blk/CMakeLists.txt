add_library(hyperdisk_blk STATIC
    blk_driver.c
    blk_io.c
    blk_ioctl.c
    blk_request.c
    blk_irp_tracker.c
    blk_fastio.c
    blk_cc_mm_sync.c
    blk_pool.c
    blk_7b_recovery.c
)

target_link_libraries(hyperdisk_blk PRIVATE hd_driver_common)
