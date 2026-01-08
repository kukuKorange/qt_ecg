cmake_minimum_required(VERSION 3.16)
project(qt_ecg LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_PREFIX_PATH "D:/qt-everywhere-src-6.9.3/qt/6.10.1/mingw_64")
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS 
    Widgets 
    Charts 
    Network 
    Sql 
    Mqtt
)

# Source files
set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mqttclient.cpp
    src/datamanager.cpp
    src/ecgchartwidget.cpp
    src/vitalschartwidget.cpp
    src/historydialog.cpp
    src/settingsdialog.cpp
    src/cloudsyncer.cpp
    src/alarmmanager.cpp
)

set(HEADERS
    src/mainwindow.h
    src/mqttclient.h
    src/datamanager.h
    src/ecgchartwidget.h
    src/vitalschartwidget.h
    src/historydialog.h
    src/settingsdialog.h
    src/cloudsyncer.h
    src/alarmmanager.h
    src/vitaldata.h
)

set(RESOURCES
    resources/resources.qrc
)

# Specify MSVC UTF-8 encoding   
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

qt_add_executable(${PROJECT_NAME}
    WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Qt6::Widgets 
    Qt6::Charts 
    Qt6::Network 
    Qt6::Sql
    Qt6::Mqtt
)
