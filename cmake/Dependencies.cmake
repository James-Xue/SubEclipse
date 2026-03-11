include_guard(GLOBAL)

find_package(X11 REQUIRED)

if (NOT X11_FOUND)
    message(FATAL_ERROR "X11 is required but was not found")
endif()

message(STATUS "Found X11: ${X11_LIBRARIES}")
