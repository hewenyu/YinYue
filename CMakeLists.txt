cmake_minimum_required(VERSION 3.10)

project(YinYue VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Core Gui Widgets Multimedia Network Test REQUIRED)

# 设置包含目录
include_directories(${CMAKE_SOURCE_DIR}/src)

set(SOURCES
    src/main.cpp
    src/ui/mainwindow.cpp
    src/core/musicplayer.cpp
    src/core/dlnamanager.cpp
    src/models/playlist.cpp
    src/models/musicfile.cpp
    src/models/lyric.cpp
    src/gui/dlnadevicedialog.cpp
)

set(HEADERS
    src/ui/mainwindow.h
    src/core/musicplayer.h
    src/core/dlnamanager.h
    src/models/playlist.h
    src/models/musicfile.h
    src/models/lyric.h
    src/gui/dlnadevicedialog.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Gui
    Qt5::Widgets
    Qt5::Multimedia
    Qt5::Network
)

# 添加测试
enable_testing()

# MusicFile 测试
add_executable(musicfile_test
    tests/musicfile_test.cpp
    src/models/musicfile.cpp
)
target_link_libraries(musicfile_test
    Qt5::Core
    Qt5::Test
    Qt5::Multimedia
)
target_include_directories(musicfile_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME musicfile_test COMMAND musicfile_test)

# MusicPlayer 测试
add_executable(musicplayer_test
    tests/musicplayer_test.cpp
    src/core/musicplayer.cpp
    src/core/dlnamanager.cpp
    src/models/playlist.cpp
    src/models/musicfile.cpp
    src/models/lyric.cpp
)
target_link_libraries(musicplayer_test
    Qt5::Core
    Qt5::Test
    Qt5::Multimedia
    Qt5::Network
)
target_include_directories(musicplayer_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME musicplayer_test COMMAND musicplayer_test)

# DLNA 测试
add_executable(dlna_test
    tests/dlna_test.cpp
    src/core/dlnamanager.cpp
    src/core/musicplayer.cpp
    src/models/playlist.cpp
    src/models/musicfile.cpp
    src/models/lyric.cpp
)
target_link_libraries(dlna_test
    Qt5::Core
    Qt5::Test
    Qt5::Multimedia
    Qt5::Network
)
target_include_directories(dlna_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME dlna_test COMMAND dlna_test)
