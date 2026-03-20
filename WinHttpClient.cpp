#include "WinHttpClient.h"
#include "FileUtils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <vector>
#include <algorithm>
#include <cctype>

using namespace std;

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

WinHttpClient::WinHttpClient() : m_hSession(nullptr) {
    m_hSession = WinHttpOpen(L"WinHTTP Downloader/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
}

WinHttpClient::~WinHttpClient() {
    if (m_hSession) {
        WinHttpCloseHandle(m_hSession);
    }
}

string WinHttpClient::WideToUTF8(const wstring& wstr) {
    if (wstr.empty()) return string();

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
        NULL, 0, NULL, NULL);

    string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(),
        &strTo[0], size_needed, NULL, NULL);

    return strTo;
}

wstring WinHttpClient::UTF8ToWide(const string& str) {
    if (str.empty()) return wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);

    wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);

    return wstrTo;
}

bool WinHttpClient::ParseUrl(const string& url, string& host, string& path,
    INTERNET_PORT& port, bool& isHttps) {

    URL_COMPONENTS urlComp = { 0 };
    urlComp.dwStructSize = sizeof(urlComp);

    wchar_t hostName[256] = { 0 };
    wchar_t urlPath[2048] = { 0 };

    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;

    wstring wurl = UTF8ToWide(url);

    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.length(), 0, &urlComp)) {
        return false;
    }

    host = WideToUTF8(hostName);
    path = WideToUTF8(urlPath);
    port = urlComp.nPort;
    isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

    return true;
}

string WinHttpClient::ExtractFilenameFromUrl(const string& url) {
    size_t pos = url.find_last_of('/');
    if (pos == string::npos) {
        return "index.html";
    }

    string filename = url.substr(pos + 1);

    // Удаление параметров запроса
    pos = filename.find('?');
    if (pos != string::npos) {
        filename = filename.substr(0, pos);
    }

    pos = filename.find('#');
    if (pos != string::npos) {
        filename = filename.substr(0, pos);
    }

    if (filename.empty()) {
        return "index.html";
    }

    return filename;
}

string WinHttpClient::ExtractFilenameFromContentDisposition(const string& contentDisposition) {
    regex filenameRegex("filename\\*?=([^;]+)");
    smatch match;

    string searchStr = contentDisposition;
    transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

    if (regex_search(searchStr, match, filenameRegex)) {
        string filename = match[1].str();

        // Удаление кавычек
        filename.erase(remove(filename.begin(), filename.end(), '"'), filename.end());
        filename.erase(remove(filename.begin(), filename.end(), '\''), filename.end());

        // Декодирование UTF-8 если есть
        if (filename.find("utf-8''") != string::npos) {
            filename = filename.substr(7);
        }

        return filename;
    }

    return "";
}

HeaderInfo WinHttpClient::ParseHeaders(const wstring& headers) {
    HeaderInfo info;
    info.contentLength = -1;

    string utf8Headers = WideToUTF8(headers);
    istringstream stream(utf8Headers);
    string line;

    while (getline(stream, line)) {
        // Удаление символов возврата каретки
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        string lowerLine = line;
        transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

        if (lowerLine.find("content-disposition:") == 0) {
            info.contentDisposition = line.substr(19);
        }
        else if (lowerLine.find("content-type:") == 0) {
            info.contentType = line.substr(13);
        }
        else if (lowerLine.find("content-length:") == 0) {
            info.contentLength = stol(line.substr(15));
        }
    }

    return info;
}

bool WinHttpClient::WriteDataToFile(const string& filepath, const vector<char>& data) {
    ofstream file(filepath, ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(data.data(), data.size());
    return file.good();
}

DownloadResult WinHttpClient::DownloadFile(const string& url, const string& outputDir) {
    DownloadResult result{ url, 0, false, "", "", {} };

    string host, path;
    INTERNET_PORT port;
    bool isHttps;

    if (!ParseUrl(url, host, path, port, isHttps)) {
        result.error = "Неверный формат URL";
        return result;
    }

    if (path.empty()) {
        path = "/";
    }

    // Открытие соединения
    HINTERNET hConnect = WinHttpConnect(m_hSession, UTF8ToWide(host).c_str(), port, 0);
    if (!hConnect) {
        result.error = "Ошибка подключения";
        return result;
    }

    // Открытие запроса
    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", UTF8ToWide(path).c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        result.error = "Ошибка создания запроса";
        return result;
    }

    // Отключение проверки сертификата для HTTPS
    if (isHttps) {
        DWORD options = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
            SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
            SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
            SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;

        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &options, sizeof(options));
    }

    // Отправка запроса
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {

        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        result.error = "Ошибка отправки запроса";
        return result;
    }

    // Получение ответа
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        result.error = "Ошибка получения ответа";
        return result;
    }

    // Получение кода статуса
    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);

    result.httpCode = statusCode;

    if (statusCode == 200) {
        // Получение заголовков
        DWORD headerSize = 0;
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &headerSize, WINHTTP_NO_HEADER_INDEX);

        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            wstring headers;
            headers.resize(headerSize / sizeof(wchar_t));

            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX, &headers[0], &headerSize, WINHTTP_NO_HEADER_INDEX)) {

                HeaderInfo headerInfo = ParseHeaders(headers);

                // Определение имени файла
                string filename;
                if (!headerInfo.contentDisposition.empty()) {
                    filename = ExtractFilenameFromContentDisposition(headerInfo.contentDisposition);
                }

                if (filename.empty()) {
                    filename = ExtractFilenameFromUrl(url);
                }

                filename = FileUtils::SanitizeFilename(filename);

                // Чтение данных
                DWORD bytesRead = 0;
                vector<char> buffer(8192);

                while (true) {
                    if (!WinHttpReadData(hRequest, buffer.data(), (DWORD)buffer.size(), &bytesRead)) {
                        break;
                    }

                    if (bytesRead == 0) {
                        break;
                    }

                    result.data.insert(result.data.end(), buffer.begin(), buffer.begin() + bytesRead);
                }

                // Сохранение в файл если указана папка
                if (!outputDir.empty()) {
                    FileUtils::CreateDirectory(outputDir);
                    filename = FileUtils::GenerateUniqueFilename(outputDir, filename);
                    string filepath = (filesystem::path(outputDir) / filename).string();

                    if (WriteDataToFile(filepath, result.data)) {
                        result.success = true;
                        result.filename = filename;
                    }
                    else {
                        result.error = "Ошибка записи файла";
                    }
                }
                else {
                    result.success = true;
                }
            }
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);

    return result;
}