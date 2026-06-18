cmake_minimum_required(VERSION 3.16)
project(robotlink_viz_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(robotlink_viz_qt WIN32
    main.cpp
    CsvReader.hpp
    Widgets.hpp
    Widgets.cpp
    MainWindow.hpp
    MainWindow.cpp
)

target_link_libraries(robotlink_viz_qt PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(WIN32)
    set(VCPKG_BIN "C:/vcpkg/installed/x64-windows/bin")
    foreach(dll harfbuzz.dll libpng16.dll md4c.dll z.dll freetype.dll double-conversion.dll pcre2-16.dll bz2.dll brotlidec.dll brotlicommon.dll zstd.dll)
        add_custom_command(TARGET robotlink_viz_qt POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${VCPKG_BIN}/${dll}"
                "$<TARGET_FILE_DIR:robotlink_viz_qt>/${dll}"
        )
    endforeach()
endif()
