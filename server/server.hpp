#pragma once
#include <socket/socket.hpp>

class Server {
    public:
        bool run(int port);

    private:
        Socket listeningSocket;
};