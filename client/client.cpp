#include <client/client.hpp>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <exception>
#include <system_error>

using namespace std;

const int timeoutSeconds = 30;

const size_t maxMessageSize = 64 * 1024;

bool Client::connectToServer(const char* ip, int port) {
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serverAddress.sin_addr) != 1) {
        cerr << "invalid server's IP address" << endl;
        return false;
    }

    if (!socket.createTCP()) {
        cerr << "failed to create client TCP socket: " << strerror(errno) << endl;
        return false;
    }

    timeval timeout{};
    timeout.tv_sec = timeoutSeconds;

    
    if (setsockopt(socket.getFd(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == -1) {
        cerr << "failed to set socket timeouts: " << strerror(errno) << endl;
        socket.close();
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

bool Client::sendMessage(const string& message) {
    if (!socket.isOpen()) {
        cerr << "Socket is not open" << endl;
        return false;
    }

    if (message.size() > maxMessageSize) {
        cerr << "Message is too large" << endl;
        return false;
    }

    string frame = message;
    frame += '\0';

    lock_guard<mutex> lock(sendMutex);

    size_t sentBytes = 0;

    while (sentBytes < frame.size()) {
        ssize_t bytes = ::send(socket.getFd(), frame.data() + sentBytes, frame.size() - sentBytes, MSG_NOSIGNAL);

        if (bytes == -1) {
            if (errno == EINTR) continue;

            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                cerr << "Sending message timed out" << endl;
                return false;
            }

            cerr << "Failed to send message: " << strerror(errno) << endl;
            return false;
        }

        if (bytes == 0) {
            cerr << "Connection has been lost" << endl;
            return false;
        }

        sentBytes += static_cast<size_t>(bytes);
    }

    return true;
}

bool Client::receiveMessages() {
    char buffer[4096];
    string pending;

    while (isRunning.load()) {
        ssize_t bytes = ::recv(socket.getFd(), buffer, sizeof(buffer), 0);

        if (bytes == -1) {
            if (errno == EINTR) continue;

            if (!isRunning.load()) return true;

            cerr << "Failed to receive message: " << strerror(errno) << endl;
            isRunning.store(false);
            return false;
        }

        if (bytes == 0) {
            if (isRunning.load()) {
                cerr << "Server disconnected" << endl;
                isRunning.store(false);
                return false;
            }

            return true;
        }

        pending.append(buffer, static_cast<size_t>(bytes));

        size_t separator;

        while ((separator = pending.find('\0')) != string::npos) {
            if (separator > maxMessageSize) {
                cerr << "Received message is too large" << endl;
                isRunning.store(false);
                return false;
            }

            string message = pending.substr(0, separator);
            pending.erase(0, separator + 1);

            cout << "\n" << message << endl;
            cout << "> " << flush;
        }

        if (pending.size() > maxMessageSize) {
            cerr << "Received message is too large" << endl;
            isRunning.store(false);
            return false;
        }
    }

    return true;
}

bool Client::runChat() {
    if (!socket.isOpen()) {
        cerr << "Cannot start chat: socket is not open" << endl;
        return false;
    }

    isRunning.store(true);

    atomic<bool> receiveSuccessful{true};
    thread receiver;

    try {
        receiver = thread([this, &receiveSuccessful] {
            receiveSuccessful.store(receiveMessages());
        });
    } catch (const system_error& error) {
        cerr << "Failed to start receiving thread: " << error.what() << endl;
        isRunning.store(false);
        return false;
    }

    cout << "Chat started!" << endl;
    cout << "Type /history to get chat history" << endl;
    cout << "Type /quit to leave the chat" << endl;

    bool isWorking = true;
    string message;

    while (isRunning.load()) {
        cout << "> " << flush;

        if (!getline(cin, message)) {
            if (cin.bad()) {
                cerr << "Failed to read message from input" << endl;
                isWorking = false;
            }

            break;
        }

        if (!isRunning.load()) break;

        if (message.empty()) continue;

        if (!sendMessage(message)) {
            isWorking = false;
            break;
        }

        if (message == "/quit") break;
    }

    isRunning.store(false); 

    if (socket.isOpen()) {
        shutdown(socket.getFd(), SHUT_RDWR);
    }

    if (receiver.joinable()) {
        receiver.join();
    }

    if (!receiveSuccessful.load()) {
        isWorking = false;
    }

    socket.close();

    return isWorking;
}