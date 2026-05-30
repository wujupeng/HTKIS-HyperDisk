add_library(hd_qos_engine STATIC
    qos_engine.cpp
    priority_queue.cpp
    token_bucket.cpp
    scheduler.cpp
)

target_link_libraries(hd_qos_engine PRIVATE hd_client_common)
