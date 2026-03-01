# rebear core library

# Source files
set(REBEAR_SOURCES
    src/escape_codec.cpp
    src/transaction.cpp
    src/patch.cpp
    src/spi_protocol.cpp
    src/patch_manager.cpp
    src/gpio_control.cpp
)

# Header files
set(REBEAR_HEADERS
    include/rebear/escape_codec.h
    include/rebear/transaction.h
    include/rebear/patch.h
    include/rebear/spi_protocol.h
    include/rebear/patch_manager.h
    include/rebear/gpio_control.h
)

# Create shared library
add_library(rebear SHARED ${REBEAR_SOURCES})

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
