#include <server/server.hpp>

#include <iostream>
#include <cerrno>
#include <cstring>

int main() {
    Server server;

    if (!server.run(8080)) {
        std::cerr << "Server stopped: " << std::strerror(errno) << std::endl;
        return 1;
    }

    return 0;
}