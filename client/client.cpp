#include <client/client.hpp>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cerrno>
#include <cstring>

using namespace std;

bool Client::connectToServer(const char* ip, int port) {
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serverAddress.sin_addr) != 1) {
        cerr << "invalid server's IP address: " << strerror(errno) << endl;
        return false;
    }

    if (!socket.createTCP()) {
        cerr << "failed to create client TCP socket: " << strerror(errno) << endl;
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