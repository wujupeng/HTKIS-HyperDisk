#include "l3_ssd_cache.h"
#include <iostream>

int main()
{
    hd::cache::L3SsdCache cache(1024ULL * 1024 * 1024 * 64);
    std::cout << "HD Cache Engine started, capacity=64GB" << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
