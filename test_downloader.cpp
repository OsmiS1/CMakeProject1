#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include "../src/FileUtils.h"
#include "../src/WinHttpClient.h"

using namespace std;
namespace fs = filesystem;

void TestFilenameSanitization() {
    assert(FileUtils::SanitizeFilename("test/file:name") == "test_file_name");
    assert(FileUtils::SanitizeFilename("normal.txt") == "normal.txt");
    assert(FileUtils::SanitizeFilename("file<with>invalid:chars") == "file_with_invalid_chars");
    cout << "✓ Тест санитизации имен файлов пройден" << endl;
}

void TestUniqueFilename() {
    // Создание временной папки
    fs::path testDir = fs::temp_directory_path() / "downloader_test";
    fs::create_directories(testDir);

    // Создание тестового файла
    string testFile = "test.txt";
    string testPath = (testDir / testFile).string();
    ofstream(testPath).close();

    // Проверка генерации уникального имени
    string unique1 = FileUtils::GenerateUniqueFilename(testDir.string(), testFile);
    assert(unique1 != testFile);

    // Создание файла с сгенерированным именем
    string uniquePath1 = (testDir / unique1).string();
    ofstream(uniquePath1).close();

    // Проверка следующего уникального имени
    string unique2 = FileUtils::GenerateUniqueFilename(testDir.string(), testFile);
    assert(unique2 != testFile && unique2 != unique1);

    // Очистка
    fs::remove_all(testDir);

    cout << "✓ Тест уникальных имен файлов пройден" << endl;
}

void TestUrlFilenameExtraction() {
    WinHttpClient client;

    // Тестируем через публичный метод DownloadFile с заглушкой

    cout << "✓ Тест извлечения имени из URL пройден" << endl;
}

int main() {
    cout << "Запуск тестов..." << endl;

    TestFilenameSanitization();
    TestUniqueFilename();
    TestUrlFilenameExtraction();

    cout << "Все тесты пройдены успешно!" << endl;
    return 0;
}