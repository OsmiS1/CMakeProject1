#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include "ThreadPool.h"
#include "WinHttpClient.h"

using namespace std;

class Downloader {
public:
    Downloader(const string& urlFile, const string& outputDir, int parallelCount);
    ~Downloader();

    void Run();

private:
    void LogStart();
    void LogEnd();
    bool ReadUrls();

    string m_urlFile;
    string m_outputDir;
    int m_parallelCount;

    atomic<int> m_completedDownloads{ 0 };
    vector<string> m_urls;
    mutex m_logMutex;

    WinHttpClient m_httpClient;
};