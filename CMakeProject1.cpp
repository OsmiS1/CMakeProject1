#define _HAS_STD_BYTE 0

#include "WinHttpClient.h"
#include "FileUtils.h"
#include "ThreadPool.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <future>
#include <windows.h>

using namespace std;

// Класс загрузчика
class Downloader {
public:
    Downloader(const string& urlFile, const string& outputDir, int parallelCount)
        : m_urlFile(urlFile), m_outputDir(outputDir), m_parallelCount(parallelCount) {

        if (parallelCount < 1 || parallelCount > 999) {
            throw invalid_argument("Количество потоков должно быть от 1 до 999");
        }
    }

    void Run() {
        LogStart();

        if (!ReadUrls()) {
            LogEnd();
            return;
        }

        ThreadPool pool(m_parallelCount);
        vector<future<DownloadResult>> results;

        for (const auto& url : m_urls) {
            auto future = pool.enqueue([this, url] {
                DownloadResult result = m_httpClient.DownloadFile(url, m_outputDir);
                {
                    lock_guard<mutex> lock(m_logMutex);
                    if (result.httpCode == 200 && result.success) {
                        Log("Завершено: " + url + " -> " + result.filename);
                    }
                    else if (result.httpCode != 200) {
                        Log("Ошибка HTTP " + to_string(result.httpCode) + " для URL: " + url);
                    }
                    else if (!result.success) {
                        cerr << "[" << FileUtils::GetCurrentTimestamp() << "] Ошибка: " << result.error << endl;
                    }
                }
                m_completedDownloads++;
                return result;
                });

            results.push_back(move(future));
        }

        for (auto& future : results) {
            future.wait();
        }

        LogEnd();
    }

private:
    void Log(const string& message) {
        cout << "[" << FileUtils::GetCurrentTimestamp() << "] " << message << endl;
    }

    void LogStart() {
        lock_guard<mutex> lock(m_logMutex);
        Log("Программа запущена");
        Log("Параметры: Файл URL: " + m_urlFile + ", Папка вывода: " + m_outputDir + ", Потоков: " + to_string(m_parallelCount));
    }

    void LogEnd() {
        lock_guard<mutex> lock(m_logMutex);
        Log("Программа завершена. Загружено файлов: " + to_string(m_completedDownloads.load()));
    }

    bool ReadUrls() {
        ifstream file(m_urlFile);
        if (!file.is_open()) {
            cerr << "[" << FileUtils::GetCurrentTimestamp() << "] Ошибка открытия файла URL: " << m_urlFile << endl;
            return false;
        }

        string url;
        while (getline(file, url)) {
            if (!url.empty() && url.back() == '\r') {
                url.pop_back();
            }
            if (!url.empty()) {
                m_urls.push_back(url);
                Log("Начало загрузки: " + url);
            }
        }

        Log("Загружено URL: " + to_string(m_urls.size()));
        return true;
    }

    string m_urlFile;
    string m_outputDir;
    int m_parallelCount;

    atomic<int> m_completedDownloads{ 0 };
    vector<string> m_urls;
    mutex m_logMutex;

    WinHttpClient m_httpClient;
};

// Настройка консоли
void SetupConsole() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hConsole, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hConsole, mode);
}

// Точка входа
int main(int argc, char* argv[]) {
    SetupConsole();

    try {
        if (argc != 4) {
            cerr << "Использование: " << argv[0]
                << " <файл_с_url> <выходная_папка> <количество_потоков>" << endl;
            cerr << "Пример: downloader.exe urls.txt C:\\downloads 5" << endl;
            return 1;
        }

        string urlFile = argv[1];
        string outputDir = argv[2];
        int parallelCount = stoi(argv[3]);

        Downloader downloader(urlFile, outputDir, parallelCount);
        downloader.Run();

    }
    catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << endl;
        return 1;
    }

    return 0;
}