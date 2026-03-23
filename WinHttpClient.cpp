#include "WinHttpClient.h"
#include "FileUtils.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>

using namespace std;

// ============================================
// Общие методы (не зависят от платформы)
// ============================================

string WinHttpClient::ExtractFilenameFromUrl(const string& url) {
    size_t pos = url.find_last_of('/');
    if (pos == string::npos) return "index.html";

    string filename = url.substr(pos + 1);
    pos = filename.find('?');
    if (pos != string::npos) filename = filename.substr(0, pos);
    pos = filename.find('#');
    if (pos != string::npos) filename = filename.substr(0, pos);

    return filename.empty() ? "index.html" : filename;
}

string WinHttpClient::ExtractFilenameFromContentDisposition(const string& contentDisposition) {
    regex filenameRegex("filename\\*?=([^;]+)");
    smatch match;
    string searchStr = contentDisposition;
    transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

    if (regex_search(searchStr, match, filenameRegex)) {
        string filename = match[1].str();
        filename.erase(remove(filename.begin(), filename.end(), '"'), filename.end());
        filename.erase(remove(filename.begin(), filename.end(), '\''), filename.end());
        if (filename.find("utf-8''") != string::npos) filename = filename.substr(7);
        return filename;
    }
    return "";
}

// ============================================
// Windows implementation
// ============================================

#ifdef _WIN32

#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

string WideToUTF8(const wstring& wstr) {
    if (wstr.empty()) return string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

wstring UTF8ToWide(const string& str) {
    if (str.empty()) return wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

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

DownloadResult WinHttpClient::DownloadFile(const string& url, const string& outputDir) {
    DownloadResult result{ url, 0, false, "", "", {} };

    string host, path;
    INTERNET_PORT port;
    bool isHttps;

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
        result.error = "Неверный формат URL";
        return result;
    }

    host = WideToUTF8(hostName);
    path = WideToUTF8(urlPath);
    port = urlComp.nPort;
    isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

    if (path.empty()) path = "/";

    HINTERNET hConnect = WinHttpConnect(m_hSession, UTF8ToWide(host).c_str(), port, 0);
    if (!hConnect) {
        result.error = "Ошибка подключения";
        return result;
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", UTF8ToWide(path).c_str(),
        NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        result.error = "Ошибка создания запроса";
        return result;
    }

    if (isHttps) {
        DWORD options = SECURITY_FLAG_IGNORE_UNKNOWN_CA |
            SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
            SECURITY_FLAG_IGNORE_CERT_CN_INVALID |
            SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &options, sizeof(options));
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        result.error = "Ошибка отправки запроса";
        return result;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        result.error = "Ошибка получения ответа";
        return result;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX);
    result.httpCode = statusCode;

    if (statusCode == 200) {
        DWORD headerSize = 0;
        WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
            WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &headerSize, WINHTTP_NO_HEADER_INDEX);

        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            wstring headers;
            headers.resize(headerSize / sizeof(wchar_t));

            if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX, &headers[0], &headerSize, WINHTTP_NO_HEADER_INDEX)) {

                string utf8Headers = WideToUTF8(headers);
                istringstream stream(utf8Headers);
                string line;
                string contentDisposition;

                while (getline(stream, line)) {
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    string lowerLine = line;
                    transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);
                    if (lowerLine.find("content-disposition:") == 0) {
                        contentDisposition = line.substr(19);
                    }
                }

                string filename;
                if (!contentDisposition.empty()) {
                    filename = ExtractFilenameFromContentDisposition(contentDisposition);
                }
                if (filename.empty()) {
                    filename = ExtractFilenameFromUrl(url);
                }
                filename = FileUtils::SanitizeFilename(filename);

                DWORD bytesRead = 0;
                vector<char> buffer(8192);

                while (true) {
                    if (!WinHttpReadData(hRequest, buffer.data(), (DWORD)buffer.size(), &bytesRead)) break;
                    if (bytesRead == 0) break;
                    result.data.insert(result.data.end(), buffer.begin(), buffer.begin() + bytesRead);
                }

                if (!outputDir.empty()) {
                    FileUtils::CreateDirectory(outputDir);
                    filename = FileUtils::GenerateUniqueFilename(outputDir, filename);
                    string filepath = (filesystem::path(outputDir) / filename).string();

                    ofstream file(filepath, ios::binary);
                    if (file.is_open()) {
                        file.write(result.data.data(), result.data.size());
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

// ============================================
// Linux implementation (POSIX sockets + OpenSSL)
// ============================================

#else

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

// Вспомогательная функция парсинга URL
bool ParseUrlLinux(const string& url, string& host, string& path, int& port, bool& isHttps) {
    isHttps = (url.find("https://") == 0);
    size_t start = url.find("://");
    if (start == string::npos) return false;
    start += 3;

    size_t end = url.find('/', start);
    if (end == string::npos) {
        host = url.substr(start);
        path = "/";
    }
    else {
        host = url.substr(start, end - start);
        path = url.substr(end);
    }

    size_t colon = host.find(':');
    if (colon != string::npos) {
        port = stoi(host.substr(colon + 1));
        host = host.substr(0, colon);
    }
    else {
        port = isHttps ? 443 : 80;
    }

    return true;
}

// Чтение заголовков HTTP ответа
string ReadHeadersLinux(int sock, SSL* ssl, string& contentDisposition, long& contentLength) {
    string response;
    char buffer[4096];
    bool headersComplete = false;

    while (!headersComplete) {
        int bytesRead;
        if (ssl) {
            bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1);
        }
        else {
            bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
        }

        if (bytesRead <= 0) break;
        buffer[bytesRead] = '\0';
        response += buffer;

        size_t headerEnd = response.find("\r\n\r\n");
        if (headerEnd != string::npos) {
            headersComplete = true;
            string headers = response.substr(0, headerEnd);
            istringstream stream(headers);
            string line;

            // Парсим статус код
            getline(stream, line);
            if (line.find("200") != string::npos) {
                contentLength = -1; // будет определено из Content-Length
            }

            // Парсим заголовки
            while (getline(stream, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                string lowerLine = line;
                transform(lowerLine.begin(), lowerLine.end(), lowerLine.begin(), ::tolower);

                if (lowerLine.find("content-disposition:") == 0) {
                    contentDisposition = line.substr(19);
                }
                else if (lowerLine.find("content-length:") == 0) {
                    contentLength = stol(line.substr(15));
                }
            }

            return response.substr(headerEnd + 4);
        }
    }
    return "";
}

WinHttpClient::WinHttpClient() : m_sslCtx(nullptr), m_ssl(nullptr), m_sock(-1) {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    m_sslCtx = SSL_CTX_new(SSLv23_client_method());
    if (m_sslCtx) {
        SSL_CTX_set_verify(m_sslCtx, SSL_VERIFY_NONE, nullptr);
    }
}

WinHttpClient::~WinHttpClient() {
    if (m_ssl) {
        SSL_shutdown(m_ssl);
        SSL_free(m_ssl);
    }
    if (m_sslCtx) {
        SSL_CTX_free(m_sslCtx);
    }
    if (m_sock != -1) {
        close(m_sock);
    }
}

DownloadResult WinHttpClient::DownloadFile(const string& url, const string& outputDir) {
    DownloadResult result{ url, 0, false, "", "", {} };

    string host, path;
    int port;
    bool isHttps;

    if (!ParseUrlLinux(url, host, path, port, isHttps)) {
        result.error = "Неверный формат URL";
        return result;
    }

    // Создание сокета
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        result.error = "Ошибка создания сокета";
        return result;
    }
    m_sock = sock;

    // Получение IP адреса
    struct hostent* server = gethostbyname(host.c_str());
    if (server == nullptr) {
        result.error = "Не удалось разрешить имя хоста: " + host;
        close(sock);
        m_sock = -1;
        return result;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr.s_addr, server->h_addr, server->h_length);
    addr.sin_port = htons(port);

    // Подключение
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        result.error = "Ошибка подключения к " + host + ":" + to_string(port);
        close(sock);
        m_sock = -1;
        return result;
    }

    SSL* ssl = nullptr;

    // SSL если нужно
    if (isHttps) {
        ssl = SSL_new(m_sslCtx);
        SSL_set_fd(ssl, sock);

        if (SSL_connect(ssl) != 1) {
            result.error = "Ошибка SSL соединения";
            SSL_free(ssl);
            close(sock);
            m_sock = -1;
            return result;
        }
        m_ssl = ssl;
    }

    // Формирование HTTP запроса
    string request = "GET " + path + " HTTP/1.1\r\n";
    request += "Host: " + host + "\r\n";
    request += "Connection: close\r\n";
    request += "User-Agent: WinDownloader/1.0\r\n";
    request += "\r\n";

    // Отправка запроса
    if (isHttps && ssl) {
        SSL_write(ssl, request.c_str(), request.length());
    }
    else {
        send(sock, request.c_str(), request.length(), 0);
    }

    // Получение ответа и парсинг заголовков
    string contentDisposition;
    long contentLength = -1;
    string body = ReadHeadersLinux(sock, ssl, contentDisposition, contentLength);

    // Определяем HTTP код из ответа
    if (body.length() > 10) {
        string firstLine = body.substr(0, body.find("\r\n"));
        if (firstLine.find("200") != string::npos) {
            result.httpCode = 200;
        }
        else if (firstLine.find("404") != string::npos) {
            result.httpCode = 404;
        }
        else if (firstLine.find("403") != string::npos) {
            result.httpCode = 403;
        }
        else if (firstLine.find("500") != string::npos) {
            result.httpCode = 500;
        }
        else {
            size_t codeStart = firstLine.find(' ');
            if (codeStart != string::npos) {
                try {
                    result.httpCode = stol(firstLine.substr(codeStart + 1, 3));
                }
                catch (...) {
                    result.httpCode = 0;
                }
            }
        }
    }
    else {
        result.httpCode = 0;
    }

    // Читаем тело ответа
    char buffer[8192];
    int bytesRead;

    // Сначала добавляем уже прочитанные данные
    result.data.insert(result.data.end(), body.begin(), body.end());

    // Читаем остальные данные
    while (true) {
        if (isHttps && ssl) {
            bytesRead = SSL_read(ssl, buffer, sizeof(buffer));
        }
        else {
            bytesRead = recv(sock, buffer, sizeof(buffer), 0);
        }
        if (bytesRead <= 0) break;
        result.data.insert(result.data.end(), buffer, buffer + bytesRead);
    }

    if (result.httpCode == 200) {
        string filename;
        if (!contentDisposition.empty()) {
            filename = ExtractFilenameFromContentDisposition(contentDisposition);
        }
        if (filename.empty()) {
            filename = ExtractFilenameFromUrl(url);
        }
        filename = FileUtils::SanitizeFilename(filename);

        if (!outputDir.empty()) {
            FileUtils::CreateDirectory(outputDir);
            filename = FileUtils::GenerateUniqueFilename(outputDir, filename);
            string filepath = (filesystem::path(outputDir) / filename).string();

            ofstream file(filepath, ios::binary);
            if (file.is_open()) {
                file.write(result.data.data(), result.data.size());
                result.success = true;
                result.filename = filename;
            }
            else {
                result.error = "Ошибка записи файла: " + filepath;
            }
        }
        else {
            result.success = true;
        }
    }
    else {
        result.error = "HTTP ошибка " + to_string(result.httpCode);
    }

    return result;
}

#endif