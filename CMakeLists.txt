cmake_minimum_required(VERSION 3.16)

project(WordsBin LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Gui Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Gui Svg)

add_executable(WordsBin
    main.cpp
    AppSettings.h
    MainWindow.cpp
    MainWindow.h
    SettingsDialog.cpp
    SettingsDialog.h
    TrayIcon.cpp
    TrayIcon.h
    CaptureTool.cpp
    CaptureTool.h
    resources.qrc
)

if(WIN32)
    target_sources(WordsBin PRIVATE app_icon.rc)
endif()

target_link_libraries(WordsBin PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Svg
    dwmapi
)

set_target_properties(WordsBin PROPERTIES OUTPUT_NAME "Words-Bin")

if(WIN32)
    set_target_properties(WordsBin PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
