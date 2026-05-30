add_executable(hd_dpdk_gateway
    main.cpp
    gateway.cpp
    packet_processor.cpp
    frame_parser.cpp
    tls_terminator.cpp
    rss_hash.cpp
)

target_link_libraries(hd_dpdk_gateway PRIVATE
    Threads::Threads
)
