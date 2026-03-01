# CLI executable
add_executable(rebear-cli
    main.cpp
)

# Link against the rebear library
target_link_libraries(rebear-cli
    PRIVATE rebear
)

# Set C++17 standard
target_compile_features(rebear-cli PRIVATE cxx_std_17)

# Install the executable
install(TARGETS rebear-cli
    RUNTIME DESTINATION bin
)
