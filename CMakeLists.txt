cmake_minimum_required(VERSION 3.10)

project(YinYue VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找Qt包
find_package(Qt5 COMPONENTS Core Widgets Multimedia REQUIRED)

# 添加源文件
add_executable(YinYue
    src/main.cpp
    src/core/musicplayer.cpp
    src/models/musicfile.cpp
    src/models/playlist.cpp
    src/models/lyric.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.ui
)

# 链接Qt库
target_link_libraries(YinYue PRIVATE
    Qt5::Core
    Qt5::Widgets
    Qt5::Multimedia
)

# 包含头文件目录
target_include_directories(YinYue PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# 添加测试
enable_testing()
add_subdirectory(tests)
