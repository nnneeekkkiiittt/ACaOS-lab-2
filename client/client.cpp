#include <client/client.hpp>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string>

using namespace std;

const int timeoutSeconds = 30;

bool Client::connectToServer(const char* ip, int port) {
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serverAddress.sin_addr) != 1) {
        cerr << "invalid server's IP address" << endl;
        return false;
    }

    if (!socket.createTCP()) {
        cerr << "failed to create client TCP socket: " << strerror(errno) << endl;
        return false;
    }

    timeval timeout{};
    timeout.tv_sec = timeoutSeconds;

    if (setsockopt(socket.getFd(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1 ||
        setsockopt(socket.getFd(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
        cerr << "failed to set socket timeouts: " << strerror(errno) << endl;
        socket.close();
        return false;
    }

    if (connect(socket.getFd(), reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) == -1) {
        cerr << "failed to connect to server: " << strerror(errno) << endl;
        socket.close();
        return false;
    }

    char serverIP[INET_ADDRSTRLEN]{};

    if (inet_ntop(AF_INET, &serverAddress.sin_addr, serverIP, sizeof(serverIP)) == nullptr) {
        cerr << "Convertation into IP failed: " << strerror(errno) << endl;
        socket.close();
        return false;
    }


    cout << "Connected to server with address " << (string)serverIP << endl; 

    return true;
}

bool Client::ping() {
        const char message[] = "ping";

        const size_t messageSize = sizeof(message) - 1; // вычитаем нулевой байт

        size_t sentBytes = 0;

        while (sentBytes < messageSize) {
            ssize_t bytes = ::send(socket.getFd(), message + sentBytes, messageSize - sentBytes, MSG_NOSIGNAL);

            if (bytes == -1) {
                if (errno == EINTR) continue;

                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    cerr << "Ping timed out" << endl;
                    return false;
                }

                cerr << "Ping failed: " << strerror(errno) << endl;

                return false;
            }

            if (bytes == 0) {
                cerr << "Connection has been lost" << endl;
                return false;
            }

            sentBytes += static_cast<size_t>(bytes);
        }

        cout << "Ping sent" << endl;

        return true;
}

bool Client::receivePong() {
    char buffer[4]{};

    size_t receivedBytes = 0;

    while (receivedBytes < sizeof(buffer)) {
        ssize_t bytes = ::recv(socket.getFd(), buffer + receivedBytes, sizeof(buffer) - receivedBytes, MSG_NOSIGNAL);

        if (bytes == -1) {
            if (errno == EINTR) continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cerr << "Pong timed out" << endl;
                return false;
            }

            cerr << "Failed to receive pong: " << strerror(errno) << endl;
            return false;
        }

        if (bytes == 0) {
            cerr << "Connection has been lost" << endl;
            return false;
        }

        receivedBytes += static_cast<size_t>(bytes);
    }

    string msg(buffer, sizeof(buffer));

    if (msg != "pong") {
        cerr << "Unknown message: " << msg << " . Should be 'pong'" << endl;
        return false;
    }

    cout << "Received message " << msg << endl;

    return true;
}