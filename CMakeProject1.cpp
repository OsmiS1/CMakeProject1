#include "Downloader.h"
#include <iostream>
#include <stdexcept>

using namespace std;

#ifdef _WIN32
#include <windows.h>

void SetupConsole() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hConsole, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hConsole, mode);
}
#else
#include <locale>
void SetupConsole() {
    setlocale(LC_ALL, "en_US.UTF-8");
}
#endif

int main(int argc, char* argv[]) {
    SetupConsole();

    try {
        if (argc != 4) {
            cerr << "Использование: " << argv[0]
                << " <файл_с_url> <выходная_папка> <количество_потоков>" << endl;
            cerr << "Пример: " << argv[0] << " urls.txt ./downloads 5" << endl;
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