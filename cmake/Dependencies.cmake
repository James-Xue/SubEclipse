include_guard(GLOBAL)

find_package(X11 REQUIRED)

if (NOT X11_FOUND)
    message(FATAL_ERROR "X11 is required but was not found")
endif()

message(STATUS "Found X11: ${X11_LIBRARIES}")

if (NOT X11_Xext_FOUND)
    message(FATAL_ERROR "Xext is required but was not found")
endif()

if (NOT X11_Xext_LIB)
    message(FATAL_ERROR "Xext library path is empty")
endif()

if (NOT X11_Xfixes_FOUND)
    message(FATAL_ERROR "Xfixes is required but was not found")
endif()

if (NOT X11_Xfixes_LIB)
    message(FATAL_ERROR "Xfixes library path is empty")
endif()

message(STATUS "Found Xext: ${X11_Xext_LIB}")
message(STATUS "Found Xfixes: ${X11_Xfixes_LIB}")
