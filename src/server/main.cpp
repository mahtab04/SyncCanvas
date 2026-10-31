#include "synccanvas/Server.hpp"
#include <cstdlib>
#include <iostream>
int main(int argc, char **argv) {
    try {
        synccanvas::ServerConfig config;
        if (const auto *key = std::getenv("SYNCCANVAS_ACCESS_KEY")) {
            config.accessKey = key;
            if (config.accessKey.size() < 16 || config.accessKey.size() > 128 ||
                config.accessKey.find_first_not_of(
                    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
                throw std::invalid_argument(
                    "SYNCCANVAS_ACCESS_KEY must contain 16-128 letters, digits, _ or -");
        }
        for (int i = 1; i < argc; ++i) {
            std::string option = argv[i];
            if (option == "--help") {
                std::cout << "synccanvas_server [--bind 127.0.0.1] [--port 8765] [--data boards]\n"
                             "Set SYNCCANVAS_ACCESS_KEY to require a shared access key.\n";
                return 0;
            }
            if (i + 1 >= argc)
                throw std::invalid_argument("Missing argument");
            std::string value = argv[++i];
            if (option == "--bind")
                config.address = value;
            else if (option == "--data")
                config.dataDirectory = value;
            else if (option == "--port") {
                std::size_t used = 0;
                int port = std::stoi(value, &used);
                if (used != value.size() || port < 1 || port > 65535)
                    throw std::invalid_argument("Invalid port");
                config.port = static_cast<unsigned short>(port);
            } else
                throw std::invalid_argument("Unknown option: " + option);
        }
        synccanvas::Server server(config);
        server.run();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
