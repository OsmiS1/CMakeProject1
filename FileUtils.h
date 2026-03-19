#pragma once
#include <string>
#include <filesystem>

using namespace std;
namespace fs = filesystem;

namespace FileUtils {
    string GetCurrentTimestamp();
    bool CreateDirectory(const string& path);
    string SanitizeFilename(const string& filename);
    string GenerateUniqueFilename(const string& directory, const string& filename);
    bool FileExists(const string& path);
    string GetFileExtension(const string& filename);
    string GetFileNameWithoutExtension(const string& filename);
}