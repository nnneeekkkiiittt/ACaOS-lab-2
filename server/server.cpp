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

const int clientTimeoutSeconds = 30;

const int shutdownWaitSeconds = 5;

volatile sig_atomic_t stopRequested = 0;

static void onSignal(int) {
    stopRequested = 1;
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
        setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &f, sizeof(f)); // ускорение за счет отключение алгоритма Нейгла

        timeval timeout{};
        timeout.tv_sec = clientTimeoutSeconds;

        if (setsockopt(clientFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1 || // таймаут на receive и на send
            setsockopt(clientFd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
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
            thread(&Server::handleClient, this, std::move(clientSocket), string(clientIP)).detach();
        } catch (const exception& error) {
            cerr << "Failed to start a thread for client " << (string)clientIP << ": " << error.what() << endl;

            lock_guard<mutex> lock(clientsMutex);
            clientFds.erase(clientFd);
        }
    }

    listeningSocket.close();

    { // после } - автоматический unlock()
        unique_lock<mutex> lock(clientsMutex);

        for (int fd : clientFds) {
            shutdown(fd, SHUT_RDWR);
        }

        if (!clientsDone.wait_for(lock, chrono::seconds(shutdownWaitSeconds), [this] { return clientFds.empty(); })) {
            cerr << "Some clients did not finish in time" << endl;
            isWorking = false;
        }
    }

    cout << "Server stopped" << endl;

    return isWorking;
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
    clientsDone.notify_all();
}

void Server::serveClient(int clientFd, const string& clientIP) {
    char buffer[4]{};

    size_t receivedBytes = 0;

    while (receivedBytes < sizeof(buffer)) {
        ssize_t bytes = ::recv(clientFd, buffer + receivedBytes, sizeof(buffer) - receivedBytes, MSG_NOSIGNAL);
        if (bytes == -1) {
            if (errno == EINTR) continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK) { // таймауты
                cerr << "Client " << clientIP << " timed out" << endl;
                return;
            }

            cerr << "Failed to receive ping from " << clientIP << ": " << strerror(errno) << endl;
            return;
        }

        if (bytes == 0) {
            cerr << "Connection with " << clientIP << " has been lost" << endl;
            return;
        }

        receivedBytes += static_cast<size_t>(bytes);
    }

    string msg(buffer, sizeof(buffer));

    cout << "Received message " << msg << " from " << clientIP << endl;

    {
        lock_guard<mutex> lock(historyMutex);

        if (!writeHistory(filename, "CLIENT " + clientIP, msg)) {
            cerr << "Failed to write message in history" << endl;
            return;
        }

        vector<string> messages;

        if (!getHistory(filename, messages)) {
            cerr << "Failed to show history" << endl;
            return;
        }

        for (const string& message : messages) {
            cout << message << endl;
        }
    }

    if (msg != "ping") {
        cerr << "Unknown message: " << msg << " from " << clientIP << ". Should be 'ping'" << endl;
        return;
    }

    const char message[] = "pong";

    size_t responseSize = sizeof(message) - 1;

    size_t sentBytes = 0;

    while (sentBytes < responseSize) {
        ssize_t bytes = ::send(clientFd, message + sentBytes, responseSize - sentBytes, MSG_NOSIGNAL);

        if (bytes == -1) {
            if (errno == EINTR) continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cerr << "Client " << clientIP << " timed out" << endl;
                return;
            }

            cerr << "Pong sending to " << clientIP << " failed: " << strerror(errno) << endl;
            return;
        }

        if (bytes == 0) {
            cerr << "Connection with " << clientIP << " has been lost" << endl;
            return;
        }

        sentBytes += static_cast<size_t>(bytes);
    }

    cout << "Successfully sent " << message << " to " << clientIP << endl;
}
