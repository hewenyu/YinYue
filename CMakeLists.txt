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
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET YinYue APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(YinYue SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
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

set_target_properties(YinYue PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS YinYue
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(YinYue)
endif()

# 添加打包配置
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "YinYue")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_RELEASE "1")
set(CPACK_PACKAGE_CONTACT "your-email@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "YinYue Music Player")
set(CPACK_PACKAGE_DESCRIPTION "A beautiful and feature-rich music player built with Qt")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6gui6, libqt6widgets6, libqt6multimedia6")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# 安装目标
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    COMPONENT applications)

# 安装桌面文件
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/yinyue.desktop.in
               ${CMAKE_CURRENT_BINARY_DIR}/yinyue.desktop @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/yinyue.desktop
        DESTINATION share/applications)

# 安装配置
if(APPLE)
    install(TARGETS ${PROJECT_NAME}
        BUNDLE DESTINATION .
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
    )
    
    # 设置 DMG 打包选项
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "YinYue")
    set(CPACK_DMG_FORMAT "UDZO")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/resources/dmg_background.png")
    set(CPACK_DMG_DS_STORE "${CMAKE_SOURCE_DIR}/resources/dmg_DS_Store")
elseif(WIN32)
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
    )
    
    # Windows NSIS 打包选项
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_PACKAGE_NAME "YinYue")
    set(CPACK_NSIS_DISPLAY_NAME "YinYue Music Player")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH ON)
else()
    install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib/static
    )
    
    # Linux DEB 打包选项
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "YinYue Team")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5multimedia5, libqt5widgets5, libqt5core5a")
endif()

# 通用打包设置
set(CPACK_PACKAGE_NAME "YinYue")
set(CPACK_PACKAGE_VENDOR "YinYue Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A beautiful music player")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "YinYue")

include(CPack)
