#pragma once
#include <string>
#include <vector>

using namespace std;

struct DownloadResult {
    string url;
    long httpCode;
    bool success;
    string filename;
    string error;
    vector<char> data;
};

class WinHttpClient {
public:
    WinHttpClient();
    ~WinHttpClient();
    DownloadResult DownloadFile(const string& url, const string& outputDir = "");

private:
    string ExtractFilenameFromUrl(const string& url);
    string ExtractFilenameFromContentDisposition(const string& contentDisposition);

#ifdef _WIN32
    void* m_hSession;  // HINTERNET
#else
    void* m_sslCtx;    // SSL_CTX*
    void* m_ssl;       // SSL*
    int m_sock;
#endif
};