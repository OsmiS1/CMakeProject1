#include "FileUtils.h"
#include <shlwapi.h>

using namespace std;
namespace fs = filesystem;

string FileUtils::GetCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    auto now_ms = chrono::time_point_cast<chrono::milliseconds>(now);
    auto value = now_ms.time_since_epoch();
    long long ms = value.count() % 1000;

    time_t now_time = chrono::system_clock::to_time_t(now);
    struct tm tm;
    localtime_s(&tm, &now_time);

    ostringstream oss;
    oss << put_time(&tm, "%Y-%m-%d %H:%M:%S");
    oss << '.' << setfill('0') << setw(3) << ms;
    return oss.str();
}

bool FileUtils::CreateDirectory(const string& path) {
    try {
        fs::create_directories(path);
        return true;
    }
    catch (const exception& e) {
        cerr << "[" << GetCurrentTimestamp() << "] Ошибка создания папки: " << e.what() << endl;
        return false;
    }
}

string FileUtils::SanitizeFilename(const string& filename) {
    string result = filename;
    regex invalid_chars("[<>:\"/\\\\|?*]");
    result = regex_replace(result, invalid_chars, "_");

    result.erase(remove_if(result.begin(), result.end(),
        [](char c) { return c < 32; }), result.end());

    return result;
}

string FileUtils::GenerateUniqueFilename(const string& directory, const string& filename) {
    fs::path filepath = fs::path(directory) / filename;

    if (!fs::exists(filepath)) {
        return filename;
    }

    string basename = filepath.stem().string();
    string extension = filepath.extension().string();

    int counter = 1;
    while (true) {
        ostringstream oss;
        oss << basename << " (" << counter << ")" << extension;
        fs::path newPath = fs::path(directory) / oss.str();

        if (!fs::exists(newPath)) {
            return oss.str();
        }
        counter++;
    }
}