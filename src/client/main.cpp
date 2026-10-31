#include "synccanvas/Application.hpp"
#include <cstdlib>
#include <iostream>
int main(int argc, char **argv) {
    try {
        std::array<std::string, 5> fields{"127.0.0.1", "8765", "studio", "designer", ""};
        if (const auto *key = std::getenv("SYNCCANVAS_ACCESS_KEY"))
            fields[4] = key;
        for (int i = 1; i < argc; ++i) {
            std::string option = argv[i];
            if (option == "--help") {
                std::cout << "synccanvas_client [--host 127.0.0.1] [--port 8765] [--room studio] [--name "
                             "designer]\nSet SYNCCANVAS_ACCESS_KEY or use the access-key field for protected "
                             "servers.\n";
                return 0;
            }
            if (i + 1 >= argc)
                throw std::invalid_argument("Missing option value");
            std::string value = argv[++i];
            if (option == "--host")
                fields[0] = value;
            else if (option == "--port")
                fields[1] = value;
            else if (option == "--room")
                fields[2] = value;
            else if (option == "--name")
                fields[3] = value;
            else
                throw std::invalid_argument("Unknown option: " + option);
        }
        synccanvas::Application application(std::move(fields));
        return application.run();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
