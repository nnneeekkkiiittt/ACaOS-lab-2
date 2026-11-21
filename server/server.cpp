#include <server/server.hpp>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <csignal>
#include <chrono>
#include <exception>
#include <thread>
#include <vector>
#include <poll.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <string>
#include <history/history.hpp>

using namespace std;

const string filename = "history.log";

const size_t maxClients = 64;

const size_t maxMessageSize = 64 * 1024;

const int clientSendTimeoutSeconds = 30;

volatile sig_atomic_t stopRequested = 0;

static void onSignal(int) {
    stopRequested = 1;
}

bool Server::sendMessage(int clientFd, const string& message) {
    if (message.size() > maxMessageSize) {
        cerr << "Message is too large" << endl;
        return false;
    }

    string frame = message;
    frame += '\0';

    size_t sentBytes = 0;

    while (sentBytes < frame.size()) {
        ssize_t bytes = ::send(clientFd, frame.data() + sentBytes, frame.size() - sentBytes, MSG_NOSIGNAL);

        if (bytes == -1) {
            if (errno == EINTR) continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cerr << "Sending message to client timed out" << endl;
                return false;
            }

            cerr << "Failed to send message to client: " << strerror(errno) << endl;
            return false;
        }

        if (bytes == 0) {
            cerr << "Connection with client has been lost" << endl;
            return false;
        }

        sentBytes += static_cast<size_t>(bytes);
    }

    return true;
}

bool Server::receiveMessage(int clientFd, string& pending, string& message, const string& clientIP) {
    while (true) {
        size_t separator = pending.find('\0');

        if (separator != string::npos) {
            if (separator > maxMessageSize) {
                cerr << "Message from " << clientIP << " is too large" << endl;
                return false;
            }

            message = pending.substr(0, separator);
            pending.erase(0, separator + 1);

            return true;
        }

        if (pending.size() > maxMessageSize) {
            cerr << "Message from " << clientIP << " is too large" << endl;
            return false;
        }

        char buffer[4096];

        ssize_t bytes = ::recv(clientFd, buffer, sizeof(buffer), 0);

        if (bytes == -1) {
            if (errno == EINTR) continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cerr << "Client " << clientIP << " timed out" << endl;
                return false;
            }

            cerr << "Failed to receive message from " << clientIP << ": " << strerror(errno) << endl;
            return false;
        }

        if (bytes == 0) {
            cerr << "Connection with " << clientIP << " has been lost" << endl;
            return false;
        }

        pending.append(buffer, static_cast<size_t>(bytes));
    }
}

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

    if (listen(listeningSocket.getFd(), SOMAXCONN) == -1) {
        cerr << "Listening failed: " << strerror(errno) << endl;
        return false;
    }

    stopRequested = 0;

    if (signal(SIGINT, onSignal) == SIG_ERR || signal(SIGTERM, onSignal) == SIG_ERR) {
        cerr << "Failed to set signal handlers: " << strerror(errno) << endl;
        return false;
    }

    cout << "Server is listening for clients on a port " << port << endl;

    bool isWorking = true;

    while (!stopRequested) {

        reapClientThreads();

        pollfd listeningPollFd{};

        listeningPollFd.fd = listeningSocket.getFd();
        listeningPollFd.events = POLLIN;

        int ready = poll(&listeningPollFd, 1, 1000); // 1 дескриптор, таймаут секунда

        if (ready == -1) { // системный сигнал
            if (errno == EINTR) continue;

            cerr << "Poll failed: " << strerror(errno) << endl;
            isWorking = false;
            break;
        }

        if (ready == 0) continue; // таймаут

        if (listeningPollFd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            cerr << "Listening socket failed" << endl;  
            isWorking = false;
        break;
        }

        if (!(listeningPollFd.revents & POLLIN)) continue;

        sockaddr_in clientAddress{};

        socklen_t sizeOfClientAddress = sizeof(clientAddress);

        int clientFd = accept(listeningSocket.getFd(), reinterpret_cast<sockaddr*>(&clientAddress), &sizeOfClientAddress);

        if (clientFd == -1) {
            if (errno == EINTR || errno == ECONNABORTED) continue;

            cerr << "Accept of a client failed: " << strerror(errno) << endl;
            isWorking = false;
            break;
        }

        Socket clientSocket(clientFd);

        int f = 1;
        if (setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &f, sizeof(f)) == -1) { // ускорение за счет отключение алгоритма Нейгла
            cerr << "Failed to set TCP_NODELAY: " << strerror(errno) << endl;
            continue;
        }

        timeval timeout{};
        timeout.tv_sec = clientSendTimeoutSeconds;

        if (setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
            cerr << "Failed to set client timeouts: " << strerror(errno) << endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN]{};

        if (inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, sizeof(clientIP)) == nullptr) {
            cerr << "Convertation into IP failed: " << strerror(errno) << endl;
            continue;
        }

        cout << "Client with address " << (string)clientIP << " connected" << endl;

        {
            lock_guard<mutex> lock(clientsMutex);

            if (clientFds.size() >= maxClients) {
                cerr << "Too many clients, rejecting " << (string)clientIP << endl;
                continue;
            }

            clientFds.insert(clientFd);
        }

        try {
            clientThreads.push_back(make_pair(clientFd, thread(&Server::handleClient, this, std::move(clientSocket), string(clientIP))));
        } catch (const exception& error) {
            cerr << "Failed to start a thread for client " << (string)clientIP << ": " << error.what() << endl;

            lock_guard<mutex> lock(clientsMutex);
            clientFds.erase(clientFd);
        }
    }

    listeningSocket.close();

    { // закрывающая } автоматически вызывает unlock
    lock_guard<mutex> lock(clientsMutex);

    for (int fd : clientFds) {
        shutdown(fd, SHUT_RDWR);
    }
    }

    for (pair<int, thread>& clientThread : clientThreads) {
        if (clientThread.second.joinable()) {
            clientThread.second.join();
        }
    }

    clientThreads.clear();

    cout << "Server stopped" << endl;

    return isWorking;
}

void Server::reapClientThreads() {
    lock_guard<mutex> lock(clientsMutex);

    auto iterator = clientThreads.begin();

    while (iterator != clientThreads.end()) {
        if (clientFds.find(iterator->first) == clientFds.end()) {
            if (iterator->second.joinable()) {
                iterator->second.join();
            }

            iterator = clientThreads.erase(iterator);
        } else {
            ++iterator;
        }
    }
}

void Server::handleClient(Socket clientSocket, string clientIP) {
    int clientFd = clientSocket.getFd();

    try {
        serveClient(clientFd, clientIP);
    } catch (const exception& error) {
        cerr << "Client " << clientIP << " failed: " << error.what() << endl;
    }

    lock_guard<mutex> lock(clientsMutex);
    clientFds.erase(clientFd);
}

void Server::serveClient(int clientFd, const string& clientIP) {
    string pending;

    while (true) {
        string message;

        if (!receiveMessage(clientFd, pending, message, clientIP)) return;

        if (message.empty()) continue;

        if (message == "/quit") return;

        if (message == "/history") {
            vector<string> messages;

            {
                lock_guard<mutex> lock(historyMutex);

                if (!getHistory(filename, messages)) {
                    cerr << "Failed to get chat history" << endl;
                    return;
                }
            }

            if (messages.empty()) {
                if (!sendMessage(clientFd, "Chat history is empty")) return;
            } else {
                for (const string& historyMessage : messages) {
                    if (!sendMessage(clientFd, historyMessage)) return;
                }
            }


            continue;
        }

        cout << "Received message " << message << " from " << clientIP << endl;

        string response = message == "ping" ? "pong" : message;
        string serverMessage = "SERVER: " + response;

        {
            lock_guard<mutex> lock(historyMutex);

            if (!writeHistory(filename, "CLIENT " + clientIP, message)) {
                cerr << "Failed to write message in history" << endl;
                return;
            }

            if (!writeHistory(filename, "SERVER", response)) {
                cerr << "Failed to write server response in history" << endl;
                return;
            }
        }

        if (!sendMessage(clientFd, serverMessage)) return;

        cout << "Successfully sent " << serverMessage << " to " << clientIP << endl;
}
}