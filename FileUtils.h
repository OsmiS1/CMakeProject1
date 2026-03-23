#pragma once

#define _HAS_STD_BYTE 0

#include <string>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <regex>
#include <windows.h>

using namespace std;
namespace fs = filesystem;

namespace FileUtils {
    string GetCurrentTimestamp();
    bool CreateDirectory(const string& path);
    string SanitizeFilename(const string& filename);
    string GenerateUniqueFilename(const string& directory, const string& filename);
}