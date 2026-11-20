#include <socket/socket.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// не передан file descryptor --> -1
Socket::Socket() {
    fd = -1;
}

Socket::Socket(int fd) {
    this->fd = fd;
}

Socket::Socket(Socket&& other) noexcept { // для обработки move(clientSocket)
    fd = other.fd;
    other.fd = -1;
}

Socket::~Socket(){
    close();
}

bool Socket::createTCP() {
    if (isOpen()) return false;

    // AF_INET - IPv4 адреса, SOCK_STREAM - TCP протокол, IPPROTO_TCP - протокол по умолчанию
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    return isOpen(); 
}

int Socket::getFd() const {
    return fd;
}

bool Socket::isOpen() const {
    return fd != -1;
}

void Socket::close() {
    if (isOpen()) {
        ::close(fd);
        fd = -1;
    }
}