#include <server/server.hpp>

#include <iostream>
#include <cerrno>
#include <cstring>

using namespace std;

int main() {
    Server server;

    if (!server.run(8080)) {
        cerr << "Server stopped because of an error" << endl;
        return 1;
    }

    return 0;
}