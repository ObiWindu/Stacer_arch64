# Qt 6 requires CMake 3.16 or newer.
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
project(Stacer)

# Adding features(build cache + faster linkers) and reasonable defaults(Debug build by default)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/cxxbasics/CXXBasics.cmake")

# Setting a cleaner directory structure for the generated binaries
set(CMAKE_BINARY_DIR        "${CMAKE_BINARY_DIR}/output")
set(EXECUTABLE_OUTPUT_PATH  "${CMAKE_BINARY_DIR}/")
set(LIBRARY_OUTPUT_PATH     "${CMAKE_BINARY_DIR}/lib")

set(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

# Minimum Qt 6 release this project builds against. The ported code needs nothing
# newer than Qt 6.0, but 6.5 is the oldest LTS that is still widely packaged, so
# it keeps distro builds working while rejecting genuinely ancient Qt6.
# The build is verified against the newest Qt6 available (6.11.x on Fedora 44).
# Override with -DQT6_MIN_VERSION=<version> to require something newer.
if(NOT QT6_MIN_VERSION)
    set(QT6_MIN_VERSION 6.5)
endif()

# Activating MOC and searching for the Qt6 dependencies
set(CMAKE_AUTOMOC ON)
find_package(Qt6 ${QT6_MIN_VERSION} COMPONENTS Core Gui Widgets Charts Svg Concurrent REQUIRED)

# Setting the minimum C++ standard and passing the Qt-specific define
set(CMAKE_CXX_STANDARD           17)
set(CMAKE_CXX_EXTENSIONS         NO)
set(CMAKE_CXX_STANDARD_REQUIRED  YES)
add_definitions(-DQT_DEPRECATED_WARNINGS)

# Subprojects
add_subdirectory(stacer-core)
add_subdirectory(stacer)
