#pragma once
#include <socket/socket.hpp>
#include <atomic>
#include <mutex>
#include <string>

class Client {
    public:
        bool connectToServer(const char* ip, int port);
        bool runChat();
    private:
        Socket socket;
        bool sendMessage(const std::string& message);
        bool receiveMessages();
        std::atomic<bool> isRunning{false};
        std::mutex sendMutex;
};