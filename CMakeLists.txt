cmake_minimum_required(VERSION 3.16)

project(SubEclipse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Dependencies.cmake)

add_library(subeclipse_core
    src/app/logger.cpp
    src/app/config.cpp
    src/render/roi.cpp
    src/platform/x11/overlay_window.cpp
    src/platform/x11/x11_capture.cpp
)

target_include_directories(subeclipse_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(subeclipse_core
    PUBLIC
        X11::X11
        ${X11_Xext_LIB}
        ${X11_Xfixes_LIB}
)

add_executable(subeclipse
    src/main.cpp
)

target_link_libraries(subeclipse
    PRIVATE
        subeclipse_core
)

message(STATUS "Configured SubEclipse with C++${CMAKE_CXX_STANDARD}")
