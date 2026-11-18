#pragma once
#include <socket/socket.hpp>

class Client {
    public:
        bool connectToServer(const char* ip, int port);
        bool ping();
    private:
        Socket socket;
};