cmake_minimum_required(VERSION 3.16...3.25)

project(YinYue VERSION 0.1 LANGUAGES CXX)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)

# Debug和Release的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/build/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/build/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/build/lib)

# 设置CMake生成文件的输出目录
set(CMAKE_BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/build)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 全局静态编译设置
set(BUILD_SHARED_LIBS OFF)
set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib ${CMAKE_FIND_LIBRARY_SUFFIXES})

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
endif()

# 设置 Qt 静态编译
set(QT_STATIC ON)
add_definitions(-DQT_STATICPLUGIN)

# macOS 特定设置
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    set(CMAKE_MACOSX_BUNDLE ON)
    set(MACOSX_BUNDLE_BUNDLE_NAME "YinYue")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.yinyue.app")
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024")
    set(MACOSX_BUNDLE_INFO_STRING "YinYue")
endif()

# 查找 Qt 组件
set(Qt_DIR "$ENV{Qt6_DIR}")
set(CMAKE_PREFIX_PATH "$ENV{Qt6_DIR}")
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Multimedia MultimediaWidgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Multimedia MultimediaWidgets Sql Core Gui Network)

# 静态插件设置（所有平台）
set(QT_PLUGINS
    Qt${QT_VERSION_MAJOR}::QGifPlugin
    Qt${QT_VERSION_MAJOR}::QICOPlugin
    Qt${QT_VERSION_MAJOR}::QJpegPlugin
)

if(WIN32)
    list(APPEND QT_PLUGINS
        Qt${QT_VERSION_MAJOR}::QWindowsIntegrationPlugin
        Qt${QT_VERSION_MAJOR}::QWindowsVistaStylePlugin
        Qt${QT_VERSION_MAJOR}::QWindowsAudioPlugin
        Qt${QT_VERSION_MAJOR}::QWindowsMediaPlugin
    )
elseif(APPLE)
    list(APPEND QT_PLUGINS
        Qt${QT_VERSION_MAJOR}::QCocoaIntegrationPlugin
        Qt${QT_VERSION_MAJOR}::QMacStylePlugin
        Qt${QT_VERSION_MAJOR}::QCocoaAudioPlugin
        Qt${QT_VERSION_MAJOR}::QAVFMediaPlugin
    )
else()
    list(APPEND QT_PLUGINS
        Qt${QT_VERSION_MAJOR}::QXcbIntegrationPlugin
    )
endif()

set(TS_FILES YinYue_zh_CN.ts)

# 源文件分组
set(SOURCES
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
    src/ui/mainwindow.ui
    src/core/musicplayer.cpp
    src/core/musicplayer.h
    src/models/musicfile.cpp
    src/models/musicfile.h
    src/models/playlist.cpp
    src/models/playlist.h
    src/models/lyric.cpp
    src/models/lyric.h
    ${TS_FILES}
)

# 头文件目录
include_directories(
    ${CMAKE_SOURCE_DIR}/src
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(YinYue
        MANUAL_FINALIZATION
        ${SOURCES}
    )
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(YinYue SHARED
            ${SOURCES}
        )
    else()
        add_executable(YinYue
            ${SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# Add include directories to the target specifically
target_include_directories(YinYue PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/models
    ${CMAKE_SOURCE_DIR}/src/ui
)

target_link_libraries(YinYue PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Network
    ${QT_PLUGINS}
)

# Windows 静态链接
if(WIN32)
    target_link_libraries(YinYue PRIVATE
        imm32.lib
        winmm.lib
        dwmapi.lib
        uxtheme.lib
        version.lib
        ws2_32.lib
        iphlpapi.lib
        userenv.lib
        netapi32.lib
        wtsapi32.lib
        setupapi.lib
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(YinYue PRIVATE
        -static-libgcc
        -static-libstdc++
        -lpthread
        -ldl
        -lrt
        -lGL
        -lX11
        -lXext
        -lXfixes
        -lXau
        -lXdmcp
        -lfontconfig
        -lfreetype
        -lexpat
        -luuid
        -lxcb
    )
endif()

# 设置目标属性
if(APPLE)
    set_target_properties(YinYue PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.yinyue.app"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024"
        MACOSX_BUNDLE_INFO_STRING "YinYue Music Player"
    )
else()
    set_target_properties(YinYue PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# 配置安装和打包
if(APPLE)
    install(TARGETS YinYue
        BUNDLE DESTINATION .
    )
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "YinYue")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/resources/dmg_background.png")
    set(CPACK_DMG_DS_STORE "${CMAKE_SOURCE_DIR}/resources/dmg_DS_Store")
elseif(WIN32)
    install(TARGETS YinYue
        RUNTIME DESTINATION bin
    )
else()
    install(TARGETS YinYue
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
    )
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "YinYue Team")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5multimedia5, libqt5widgets5, libqt5core5a")
    
    # 安装桌面文件（仅用于 Linux）
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/yinyue.desktop.in
                   ${CMAKE_CURRENT_BINARY_DIR}/yinyue.desktop @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/yinyue.desktop
            DESTINATION share/applications)
endif()

# 通用打包设置
set(CPACK_PACKAGE_NAME "YinYue")
set(CPACK_PACKAGE_VENDOR "YinYue Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A beautiful music player")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "YinYue")

include(CPack)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(YinYue)
endif()
