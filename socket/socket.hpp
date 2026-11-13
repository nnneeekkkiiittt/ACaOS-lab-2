#pragma once

class Socket {
    public:
        Socket();

        explicit Socket(int fd);

        ~Socket();

        Socket(const Socket&) = delete; //сокет с одним и тем же fd ни в коем случае нельзя копировать
        Socket& operator = (const Socket&) = delete;

        bool createTCP();

        int getFd();

        bool isOpen();

        void close();

    private:
        int fd;


};

