#pragma once
#include <socket/socket.hpp>
#include <condition_variable>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class Server {
    public:
        bool run(int port);

    private:
        void handleClient(Socket clientSocket, std::string clientIP);
        void serveClient(int clientFd, const std::string& clientIP);
        void reapClientThreads();

        bool sendMessage(int clientFd, const std::string& message);
        bool receiveMessage(int clientFd, std::string& pending, std::string& message, const std::string& clientIP); 

        Socket listeningSocket;

        std::mutex clientsMutex;
        std::set<int> clientFds;
        std::vector<std::pair<int, std::thread>> clientThreads;
        std::mutex historyMutex;
};
