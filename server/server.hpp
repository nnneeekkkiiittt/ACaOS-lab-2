#pragma once
#include <socket/socket.hpp>
#include <condition_variable>
#include <mutex>
#include <set>
#include <string>

class Server {
    public:
        bool run(int port);

    private:
        void handleClient(Socket clientSocket, std::string clientIP);
        void serveClient(int clientFd, const std::string& clientIP);

        Socket listeningSocket;

        std::mutex clientsMutex;
        std::condition_variable clientsDone;
        std::set<int> clientFds;

        std::mutex historyMutex;
};
