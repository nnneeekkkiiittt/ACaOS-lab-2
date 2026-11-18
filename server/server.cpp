#include <server/server.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <string>

using namespace std;

bool Server::run(int port) {
    if (!listeningSocket.createTCP()) {
        cerr << "Failed to initialize TCP socket" << endl;
        return false;
    }

    int reuse_allowed = 1;

    // включаем переиспользование локального адреса
    if (setsockopt(listeningSocket.getFd(), SOL_SOCKET, SO_REUSEADDR, &reuse_allowed, sizeof(reuse_allowed)) == -1) {
        cerr << "Setsockopt failed: " << strerror(errno) << endl;
        return false;
    }

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET; //ipv4
    serverAddress.sin_port = htons(port); //htons (host to network short) - потому что в сетевых протоколах старший байт идет первым
    serverAddress.sin_addr.s_addr = INADDR_ANY; // все локальные ip адреса

    if (bind(listeningSocket.getFd(), reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
        cerr << "Listening failed: " << strerror(errno) << endl;
        return false;
    }

    if (listen(listeningSocket.getFd(), 1) == -1) { // 1 - параметр backlog
        cerr << "Listening failed: " << strerror(errno) << endl;
    }

    cout << "Server is listening for a client on a port " << port << endl;

    sockaddr_in clientAddress{};

    socklen_t sizeOfClientAddress = sizeof(clientAddress);

    int cliendFd = accept(listeningSocket.getFd(), reinterpret_cast<sockaddr*>(&clientAddress), &sizeOfClientAddress);
    
    if(cliendFd == -1) {
        cerr << "Accept of a client failed: " << strerror(errno) << endl;
        return false;
    }

    Socket clientSocket(cliendFd);

    int f = 1;
    setsockopt(cliendFd, IPPROTO_TCP, TCP_NODELAY, &f, sizeof(f)); // ускорение за счет отключение алгоритма Нейгла

    listeningSocket.close();

    char clientIP[INET_ADDRSTRLEN]{};

    if (inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, sizeof(clientIP)) == nullptr) {
        cerr << "Convertation into IP failed: " << strerror(errno) << endl;
        clientSocket.close();
        return false;
    }

    cout << "Client with address " << (string)clientIP << " connected; listening socket is closing" << endl;

    char buffer[4]{};

    size_t receivedBytes = 0;

    while (receivedBytes < sizeof(buffer)) {
        ssize_t bytes = ::recv(cliendFd, buffer + receivedBytes, sizeof(buffer) - receivedBytes, MSG_NOSIGNAL);
        if (bytes == -1) {
            if (errno == EINTR) continue;

            cerr << "Failed to receive ping: " << strerror(errno) << endl;
            return false;
        }

        if (bytes == 0) {
            cerr << "Connection has been lost" << endl;
            return false;
        }

        receivedBytes += static_cast<size_t>(bytes);
    }

    cout << "Received message " << string(buffer, sizeof(buffer)) << endl;

    return true;
}