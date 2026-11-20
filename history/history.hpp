#pragma once
#include <string>
#include <vector>

using namespace std;

bool writeHistory(const string& filename, const string& author, const string& message);

bool getHistory(const string& filename, vector<string>& messages);