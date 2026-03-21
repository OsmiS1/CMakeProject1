#include "Downloader.h"
#include "FileUtils.h"
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

Downloader::Downloader(const string& urlFile, const string& outputDir, int parallelCount)
    : m_urlFile(urlFile), m_outputDir(outputDir), m_parallelCount(parallelCount) {

    if (parallelCount < 1 || parallelCount > 999) {
        throw invalid_argument("Количество потоков должно быть от 1 до 999");
    }
}

Downloader::~Downloader() {
}

void Downloader::LogStart() {
    lock_guard<mutex> lock(m_logMutex);
    cout << "[" << FileUtils::GetCurrentTimestamp() << "] Программа запущена" << endl;
    cout << "[" << FileUtils::GetCurrentTimestamp() << "] Параметры: "
        << "Файл URL: " << m_urlFile
        << ", Папка вывода: " << m_outputDir
        << ", Потоков: " << m_parallelCount << endl;
}

void Downloader::LogEnd() {
    lock_guard<mutex> lock(m_logMutex);
    cout << "[" << FileUtils::GetCurrentTimestamp() << "] Программа завершена. "
        << "Загружено файлов: " << m_completedDownloads.load() << endl;
}

bool Downloader::ReadUrls() {
    ifstream file(m_urlFile);
    if (!file.is_open()) {
        cerr << "[" << FileUtils::GetCurrentTimestamp() << "] Ошибка открытия файла URL: " << m_urlFile << endl;
        return false;
    }

    string url;
    while (getline(file, url)) {
        // Удаление символа возврата каретки для Windows
        if (!url.empty() && url.back() == '\r') {
            url.pop_back();
        }

        if (!url.empty()) {
            m_urls.push_back(url);
        }
    }

    cout << "[" << FileUtils::GetCurrentTimestamp() << "] Загружено URL: " << m_urls.size() << endl;
    return true;
}

DownloadResult Downloader::DownloadFile(const string& url) {
    {
        lock_guard<mutex> lock(m_logMutex);
        cout << "[" << FileUtils::GetCurrentTimestamp() << "] Начало загрузки: " << url << endl;
    }

    DownloadResult result = m_httpClient.DownloadFile(url, m_outputDir);

    {
        lock_guard<mutex> lock(m_logMutex);
        if (result.httpCode == 200 && result.success) {
            cout << "[" << FileUtils::GetCurrentTimestamp() << "] Завершено: " << url
                << " -> " << result.filename << endl;
        }
        else if (result.httpCode != 200) {
            cout << "[" << FileUtils::GetCurrentTimestamp() << "] Ошибка HTTP " << result.httpCode
                << " для URL: " << url << endl;
        }
        else if (!result.success) {
            cerr << "[" << FileUtils::GetCurrentTimestamp() << "] Ошибка загрузки " << url
                << ": " << result.error << endl;
        }
    }

    return result;
}

void Downloader::Run() {
    LogStart();

    if (!ReadUrls()) {
        LogEnd();
        return;
    }

    // Создание пула потоков
    ThreadPool pool(m_parallelCount);
    vector<future<DownloadResult>> results;

    // Запуск задач загрузки
    for (const auto& url : m_urls) {
        auto future = pool.enqueue([this, url] {
            m_activeDownloads++;
            DownloadResult result = DownloadFile(url);
            m_activeDownloads--;
            m_completedDownloads++;
            return result;
            });

        results.push_back(move(future));
    }

    // Ожидание завершения всех загрузок
    for (auto& future : results) {
        future.wait();
    }

    LogEnd();
}