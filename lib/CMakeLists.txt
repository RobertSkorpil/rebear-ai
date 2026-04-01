# rebear core library

# Platform-independent source files
set(REBEAR_SOURCES
    src/escape_codec.cpp
    src/transaction.cpp
    src/patch.cpp
    src/protocol.cpp
    src/network_client.cpp
    src/spi_protocol_network.cpp
    src/gpio_control_network.cpp
)

# Linux-specific hardware interface files
if(UNIX AND NOT APPLE)
    list(APPEND REBEAR_SOURCES
        src/spi_protocol.cpp
        src/gpio_control.cpp
    )
endif()

# Header files
set(REBEAR_HEADERS
    include/rebear/escape_codec.h
    include/rebear/transaction.h
    include/rebear/patch.h
    include/rebear/spi_protocol.h
    include/rebear/gpio_control.h
    include/rebear/protocol.h
    include/rebear/network_client.h
    include/rebear/spi_protocol_network.h
    include/rebear/gpio_control_network.h
)

# Create library (shared on Linux, static on Windows for simplicity)
if(WIN32)
    add_library(rebear STATIC ${REBEAR_SOURCES})
else()
    add_library(rebear SHARED ${REBEAR_SOURCES})
endif()

# Set library properties
set_target_properties(rebear PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    PUBLIC_HEADER "${REBEAR_HEADERS}"
)

# Include directories
target_include_directories(rebear
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link libraries
target_link_libraries(rebear
    PUBLIC
        Threads::Threads
)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(rebear
        PUBLIC
            ws2_32
    )
endif()

# Installation
install(TARGETS rebear
    EXPORT rebearTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/rebear
)

# Install export targets
install(EXPORT rebearTargets
    FILE rebearTargets.cmake
    NAMESPACE rebear::
    DESTINATION lib/cmake/rebear
)

# Create and install package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/rebearConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/rebearConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/rebearConfig.cmake"
    INSTALL_DESTINATION lib/cmake/rebear
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/rebearConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/rebearConfigVersion.cmake"
    DESTINATION lib/cmake/rebear
)
