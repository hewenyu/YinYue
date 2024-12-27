cmake_minimum_required(VERSION 3.10)

project(YinYue VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 添加 pkg-config 支持
find_package(PkgConfig REQUIRED)

# 使用 pkg-config 查找 TagLib
pkg_check_modules(TAGLIB REQUIRED taglib)

find_package(Qt5 COMPONENTS Core Gui Widgets Multimedia Network Xml REQUIRED)

# 设置包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${TAGLIB_INCLUDE_DIRS}
)

# 设置链接目录
link_directories(${TAGLIB_LIBRARY_DIRS})

set(SOURCES
    src/main.cpp
    src/ui/mainwindow.cpp
    src/core/musicplayer.cpp
    src/core/dlnamanager.cpp
    src/core/localplayer.cpp
    src/models/playlist.cpp
    src/models/musicfile.cpp
    src/models/lyric.cpp
    src/gui/dlnadevicedialog.cpp
    src/models/dlnadevice.cpp
)

set(HEADERS
    src/ui/mainwindow.h
    src/core/musicplayer.h
    src/core/dlnamanager.h
    src/core/localplayer.h
    src/models/playlist.h
    src/models/musicfile.h
    src/models/lyric.h
    src/gui/dlnadevicedialog.h
    src/models/dlnadevice.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Multimedia
    Qt5::Network
    Qt5::Xml
    ${TAGLIB_LIBRARIES}
)

# 添加编译标志
target_compile_options(${PROJECT_NAME} PRIVATE ${TAGLIB_CFLAGS_OTHER})
