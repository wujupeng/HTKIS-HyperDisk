add_executable(hd_boot_agent
    main.cpp
    dhcp_client.cpp
    http_boot.cpp
    ipxe_chain.cpp
    chunk_stream.cpp
    integrity_check.cpp
    boot_meta.cpp
)

target_link_libraries(hd_boot_agent PRIVATE hd_client_common)
