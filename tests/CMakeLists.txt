cmake_minimum_required(VERSION 3.10)

# 查找Qt5包
find_package(Qt5 COMPONENTS Core Test Multimedia Network REQUIRED)

# 启用测试
enable_testing()

# 添加测试可执行文件
add_executable(dlna_test
    dlna_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/dlnamanager.cpp
)

add_executable(musicfile_test
    musicfile_test.cpp
    ${CMAKE_SOURCE_DIR}/src/models/musicfile.cpp
)

add_executable(musicplayer_test
    musicplayer_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/musicplayer.cpp
    ${CMAKE_SOURCE_DIR}/src/models/playlist.cpp
    ${CMAKE_SOURCE_DIR}/src/models/musicfile.cpp
)

# 链接Qt库
target_link_libraries(dlna_test PRIVATE
    Qt5::Core
    Qt5::Test
    Qt5::Network
)

target_link_libraries(musicfile_test PRIVATE
    Qt5::Core
    Qt5::Test
    Qt5::Multimedia
)

target_link_libraries(musicplayer_test PRIVATE
    Qt5::Core
    Qt5::Test
    Qt5::Multimedia
)

# 包含头文件目录
target_include_directories(dlna_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(musicfile_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_include_directories(musicplayer_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# 添加测试
add_test(NAME dlna_test COMMAND dlna_test)
add_test(NAME musicfile_test COMMAND musicfile_test)
add_test(NAME musicplayer_test COMMAND musicplayer_test) 