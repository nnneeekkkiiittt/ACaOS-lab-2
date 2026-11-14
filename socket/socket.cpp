#include <socket.hpp>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// не передан file descryptor --> -1
Socket::Socket() {
    fd = -1;
}

Socket::Socket(int fd) {
    fd = fd;
}

Socket::~Socket(){
    close();
}

bool Socket::createTCP() {
    if (isOpen()) return false;

    // AF_INET - IPv4 адреса, SOCK_STREAM - TCP протокол, 0 - протокол по умолчанию
    fd = socket(AF_INET, SOCK_STREAM, 0);
    return isOpen(); 
}

int Socket::getFd() {
    return fd;
}

bool Socket::isOpen() {
    return fd != -1;
}

void Socket::close() {
    if (isOpen()) {
        ::close(fd);
        fd = -1;
    }
}