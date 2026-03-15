cmake_minimum_required(VERSION 3.16)

project(SnipLite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Gui)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Gui)

add_executable(SnipLite
    main.cpp
    MainWindow.cpp
    MainWindow.h
    TrayIcon.cpp
    TrayIcon.h
    CaptureTool.cpp
    CaptureTool.h
)

target_link_libraries(SnipLite PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Gui
)

if(WIN32)
    set_target_properties(SnipLite PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
