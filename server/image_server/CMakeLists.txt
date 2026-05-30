add_executable(hd_image_server
    main.cpp
    image_service.cpp
    block_handler.cpp
    chunk_index.cpp
    io_engine.cpp
    grpc_service.cpp
)

target_link_libraries(hd_image_server PRIVATE
    Boost::system
    Boost::program_options
    Boost::filesystem
    Threads::Threads
)
