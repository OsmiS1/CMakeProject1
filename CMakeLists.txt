cmake_minimum_required(VERSION 3.10)
project(WinDownloader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Отключаем std::byte для Windows
add_definitions(-D_HAS_STD_BYTE=0)

# Include current directory for headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Общие исходники
set(SOURCES
    CMakeProject1.cpp
    Downloader.cpp
    FileUtils.cpp
    WinHttpClient.cpp
)

# Main executable
add_executable(downloader ${SOURCES})

# Кроссплатформенные библиотеки
if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
    target_link_libraries(downloader
        winhttp
        crypt32
        shell32
        shlwapi
    )
else()
    # Linux: используем pthread и OpenSSL для HTTPS
    find_package(OpenSSL REQUIRED)
    target_link_libraries(downloader
        pthread
        ${OPENSSL_LIBRARIES}
    )
    target_include_directories(downloader PRIVATE ${OPENSSL_INCLUDE_DIR})
    add_definitions(-D_LINUX)
endif()