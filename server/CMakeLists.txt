# rebear-server daemon

# Source files
set(SERVER_SOURCES
    main.cpp
    network_server.cpp
    command_handler.cpp
)

# Create executable
add_executable(rebear-server ${SERVER_SOURCES})

# Include directories
target_include_directories(rebear-server
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link libraries
target_link_libraries(rebear-server
    PRIVATE
        rebear
        Threads::Threads
)

# Installation
install(TARGETS rebear-server
    RUNTIME DESTINATION bin
)

# Install systemd service file (optional)
if(UNIX AND NOT APPLE)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/rebear-server.service.in)
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/rebear-server.service.in
            ${CMAKE_CURRENT_BINARY_DIR}/rebear-server.service
            @ONLY
        )
        
        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rebear-server.service
            DESTINATION /etc/systemd/system
            OPTIONAL
        )
    endif()
endif()
