add_executable(hd_boot_agent
    main.cpp
    dhcp_client.cpp
    http_boot.cpp
    ipxe_chain.cpp
    chunk_stream.cpp
    integrity_check.cpp
    boot_meta.cpp
    com1_logger.cpp
)

target_compile_definitions(hd_boot_agent PRIVATE
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(hd_boot_agent PRIVATE
    hd_client_common
    ws2_32
    iphlpapi
)

if(MSVC)
    target_compile_options(hd_boot_agent PRIVATE /W4)
endif()
