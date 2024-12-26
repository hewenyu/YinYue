cmake_minimum_required(VERSION 3.16...3.25)

project(YinYue VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS 特定设置
if(APPLE)
    set(CMAKE_MACOSX_BUNDLE ON)
    set(MACOSX_BUNDLE_BUNDLE_NAME "YinYue")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.yinyue.app")
    set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2024")
    set(MACOSX_BUNDLE_INFO_STRING "YinYue")
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Multimedia MultimediaWidgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Multimedia MultimediaWidgets Sql)

set(TS_FILES YinYue_zh_CN.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
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

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(YinYue
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(YinYue SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(YinYue
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(YinYue PRIVATE 
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Multimedia
    Qt${QT_VERSION_MAJOR}::MultimediaWidgets
)

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
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
    )
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "YinYue")
    set(CPACK_NSIS_DISPLAY_NAME "YinYue Music Player")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
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
