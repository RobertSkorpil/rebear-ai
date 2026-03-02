cmake_minimum_required(VERSION 3.10)
project(rebear VERSION 1.0.0 LANGUAGES CXX)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Build options
option(BUILD_CLI "Build command-line utility" ON)
option(BUILD_GUI "Build Qt GUI application" ON)
option(BUILD_SERVER "Build network server daemon" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# Find dependencies
find_package(Threads REQUIRED)

# Find Qt5 if GUI is enabled
if(BUILD_GUI)
    find_package(Qt5 COMPONENTS Core Widgets Charts QUIET)
    if(NOT Qt5_FOUND)
        message(WARNING "Qt5 not found. GUI application will not be built.")
        set(BUILD_GUI OFF)
    endif()
endif()

# Add subdirectories
add_subdirectory(lib)

if(BUILD_CLI)
    add_subdirectory(cli)
endif()

if(BUILD_GUI AND Qt5_FOUND)
    add_subdirectory(gui)
endif()

if(BUILD_SERVER)
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
