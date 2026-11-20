#include <client/client.hpp>
#include <iostream>
#include <cerrno>
#include <cstring>

using namespace std;

int main() {
    const char* serverIP = "127.0.0.1";
    int port = 8080;
    
    cout << "trying to connect to server with address " << (string)serverIP << " on port " << port << endl;

    Client client;

    if (!client.connectToServer(serverIP, port)) {
        cerr << "failed to connect to server" << endl;
        return 1;
    }

    cout << "succesfully connected to server" << endl;

    if (!client.ping()) {
        cerr << "Failed to send ping" << endl;
        return 1;
    }
    
    if (!client.receivePong()) {
        cerr << "Failed to receive pong" << endl;
        return 1;
    }

    return 0;
}