#pragma once
#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#include <functional>

using namespace std;

struct DownloadResult {
    string url;
    long httpCode;
    bool success;
    string filename;
    string error;
    vector<char> data;
};

struct HeaderInfo {
    string contentDisposition;
    string contentType;
    long contentLength;
};

class WinHttpClient {
public:
    WinHttpClient();
    ~WinHttpClient();

    DownloadResult DownloadFile(const string& url, const string& outputDir = "");

private:
    bool ParseUrl(const string& url, string& host, string& path, INTERNET_PORT& port, bool& isHttps);
    string ExtractFilenameFromUrl(const string& url);
    string ExtractFilenameFromContentDisposition(const string& contentDisposition);
    HeaderInfo ParseHeaders(const wstring& headers);
    string WideToUTF8(const wstring& wstr);
    wstring UTF8ToWide(const string& str);
    bool WriteDataToFile(const string& filepath, const vector<char>& data);

    HINTERNET m_hSession;
};
