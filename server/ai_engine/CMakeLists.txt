add_executable(hd_ai_engine
    main.cpp
    ai_service.cpp
    access_collector.cpp
    hotblock_rank.cpp
    prefetch_scheduler.cpp
)

target_link_libraries(hd_ai_engine PRIVATE
    Boost::system
    Threads::Threads
)
