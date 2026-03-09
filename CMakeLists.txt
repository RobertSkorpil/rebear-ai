cmake_minimum_required(VERSION 3.10)
project(rebear VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # GCC and Clang flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # MSVC flags
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Od")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
endif()

# Build options
option(BUILD_CLI "Build command-line utility" ON)
option(BUILD_GUI "Build Qt GUI application" ON)
option(BUILD_SERVER "Build network server daemon" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# Find dependencies
find_package(Threads REQUIRED)

# Find Qt if GUI is enabled (prefer Qt6, fallback to Qt5)
if(BUILD_GUI)
    # Add QTDIR to CMAKE_PREFIX_PATH if set
    if(DEFINED ENV{QTDIR})
        list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}")
        message(STATUS "Using QTDIR: $ENV{QTDIR}")
    endif()
    
    find_package(Qt6 COMPONENTS Core Widgets Charts QUIET)
    if(Qt6_FOUND)
        set(QT_VERSION_MAJOR 6)
        message(STATUS "Found Qt6: ${Qt6_VERSION}")
    else()
        find_package(Qt5 COMPONENTS Core Widgets Charts QUIET)
        if(Qt5_FOUND)
            set(QT_VERSION_MAJOR 5)
            message(STATUS "Found Qt5: ${Qt5_VERSION}")
        else()
            message(WARNING "Qt5 or Qt6 not found. GUI application will not be built.")
            message(STATUS "  Searched paths: ${CMAKE_PREFIX_PATH}")
            set(BUILD_GUI OFF)
        endif()
    endif()
endif()

# Add subdirectories
add_subdirectory(lib)

# CLI requires Linux hardware interfaces
if(BUILD_CLI AND UNIX AND NOT APPLE)
    add_subdirectory(cli)
endif()

if(BUILD_GUI AND (Qt5_FOUND OR Qt6_FOUND))
    add_subdirectory(gui)
endif()

# Server requires Linux hardware interfaces
if(BUILD_SERVER AND UNIX AND NOT APPLE)
    add_subdirectory(server)
endif()

# Installation
install(DIRECTORY docs/ DESTINATION share/doc/rebear
    FILES_MATCHING PATTERN "*.md")

# Print configuration summary
message(STATUS "")
message(STATUS "rebear Configuration Summary:")
message(STATUS "  Version:           ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:      C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  Build CLI:         ${BUILD_CLI}")
message(STATUS "  Build GUI:         ${BUILD_GUI}")
message(STATUS "  Build Server:      ${BUILD_SERVER}")
message(STATUS "  Build Tests:       ${BUILD_TESTS}")
message(STATUS "")
