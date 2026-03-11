cmake_minimum_required(VERSION 3.16)

project(SubEclipse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/Dependencies.cmake)

add_library(subeclipse_core
    src/app/logger.cpp
    src/app/config.cpp
)

target_include_directories(subeclipse_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(subeclipse
    src/main.cpp
)

target_link_libraries(subeclipse
    PRIVATE
        subeclipse_core
        X11::X11
)

message(STATUS "Configured SubEclipse with C++${CMAKE_CXX_STANDARD}")
