#include <server.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <cerrno>
#include <cstring>


bool Server::run(int port) {
    if (!listeningSocket.createTCP()) {
        std:cerr << "Failed to initialize TCP socket" << endl;
        return false;
    }

    int reuse_allowed = 1;

    if (setsockopt(listeningSocket.getFd(), SOL_SOCKET, SO_REUSEADDR, &reuse_allowed, sizeof(reuse_allowed)) == -1) {
        std::cerr << "Setsockopt failed: " << std::strerror(errno) << endl;
        return false;
    }

    sockaddr_in address{};

    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(listeningSocket.getFd(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) == -1) {
        std::cerr << "Listening failed: " << std::strerror(errno) << endl;
        return false;
    }

    std::cout << "Server is listening for a client on a port " << port << endl;

    int cliendFd = accept(listeningSocket.getFd(), nullptr, nullptr);
    
    if(cliendFd == -1) {
        std::cerr << "Accept of a client failed: " << std::strerror(errno) << endl;
        return false;
    }

    Socket clientSocket(cliendFd);

    listeningSocket.close();

    std::cout << "Client connected; listening socket is closing" << endl;

    return true;
}