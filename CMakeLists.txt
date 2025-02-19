cmake_minimum_required(VERSION 3.16)

project(ADB_helper VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt6 modules
find_package(Qt6 COMPONENTS Widgets Multimedia MultimediaWidgets REQUIRED)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    auto_connect.py
)

# Create the executable using Qt6
qt_add_executable(ADB_helper
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

# Link against Qt6 modules
target_link_libraries(ADB_helper PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)

set_target_properties(ADB_helper PROPERTIES
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS ADB_helper
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

qt_finalize_executable(ADB_helper)
