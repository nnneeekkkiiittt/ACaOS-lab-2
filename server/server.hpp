#pragma once
#include <socket/socket.h>

class Server {
    public:
        bool run(int port);

    private:
        Socket listeningSocket;
};