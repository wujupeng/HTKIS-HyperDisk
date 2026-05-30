add_executable(hd_cache_engine
    main.cpp
    cache_service.cpp
    l3_ssd_cache.cpp
    arc_policy.cpp
    io_scheduler.cpp
)

target_link_libraries(hd_cache_engine PRIVATE
    Boost::system
    Boost::program_options
    Threads::Threads
)
