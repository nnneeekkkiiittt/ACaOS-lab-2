#include <history/history.hpp>
#include <string>
#include <fstream>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <iomanip>
#include <filesystem>

using namespace std;

bool writeHistory(const string& filename, const string& author, const string& message) {
    ofstream historyFile(filename, std::ios::app);
    
    if (!historyFile.is_open()) {
        cerr << "Failed to open file " << filename << endl;
        return false;
    }

    time_t time = std::time(nullptr);
    tm* localTime = localtime(&time);
    if (localtime == nullptr) {
        cerr << "Failed to fetch local time" << endl;
        return false;
    }

    historyFile << "[" << put_time(localTime, "%Y-%m-%d %H:%M:%S") << "] " << author << ": " << message << endl;

    historyFile.flush();

    if (!historyFile) {
        cerr << "Failed to write " << filename << endl;
        return false;
    }

    return true;

}

bool getHistory(const string& filename, vector<string>& messages) {
    messages.clear();

    error_code error;

    bool flag = filesystem::exists(filename, error);

    if (error) {
        cerr << "Failed to read " << filename << " : " << error.message() << endl;
        return false;
    }

    if (!flag) return true;

    ifstream historyFile(filename);

    if (!historyFile.is_open()) {
        cerr << "Failed to open " << filename << endl;
        return false;
    }

    string line;

    while (getline(historyFile, line)) {
        messages.push_back(line);
    }

    if (historyFile.bad()) {
        cerr << "Failed to read " << filename << endl;
        return false;
    }

    return true;
}