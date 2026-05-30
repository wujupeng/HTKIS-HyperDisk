add_library(hd_smart_cache STATIC
    smart_cache.cpp
    l1_ram_cache.cpp
    l2_nvme_cache.cpp
    cache_policy.cpp
    arc_policy.cpp
    w_tinylfu.cpp
)

target_link_libraries(hd_smart_cache PRIVATE hd_client_common)
