#include "image_service.h"
#include <iostream>
#include <csignal>

static hd::image::ImageService* g_service = nullptr;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        delete g_service;
        g_service = nullptr;
        exit(0);
    }
}

int main(int argc, char* argv[])
{
    std::string data_dir = "./data/images";
    if (argc > 1) data_dir = argv[1];

    hd::image::ImageService service(data_dir);
    g_service = &service;

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "HD Image Server started, data_dir=" << data_dir << std::endl;

    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
