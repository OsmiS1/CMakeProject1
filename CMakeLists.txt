cmake_minimum_required(VERSION 3.10)
project(WinDownloader VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-D_HAS_STD_BYTE=0)

if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(downloader
    CMakeProject1.cpp
    FileUtils.cpp
    WinHttpClient.cpp
)

target_link_libraries(downloader
    winhttp
    crypt32
    shell32
    shlwapi
)