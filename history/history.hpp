#pragma once
#include <string>
#include <vector>

bool writeHistory(const std::string& filename, const std::string& author, const std::string& message);

bool getHistory(const std::string& filename, std::vector<std::string>& messages);