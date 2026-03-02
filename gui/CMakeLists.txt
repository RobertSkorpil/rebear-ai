# rebear Qt GUI application

# Enable Qt MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Source files
set(GUI_SOURCES
    main.cpp
    mainwindow.cpp
    widgets/transaction_viewer.cpp
    widgets/patch_editor.cpp
    widgets/hex_viewer.cpp
    widgets/connection_dialog.cpp
    # TODO: Uncomment as implemented
    # widgets/address_visualizer.cpp
)

# Header files
set(GUI_HEADERS
    mainwindow.h
    widgets/transaction_viewer.h
    widgets/patch_editor.h
    widgets/hex_viewer.h
    widgets/connection_dialog.h
    # TODO: Uncomment as implemented
    # widgets/address_visualizer.h
)

# UI files (none yet, using code-based UI)
set(GUI_UIS
    # TODO: Uncomment as implemented
    # mainwindow.ui
)

# Create executable
add_executable(rebear-gui ${GUI_SOURCES} ${GUI_HEADERS} ${GUI_UIS})

# Link against core library and Qt
target_link_libraries(rebear-gui
    PRIVATE
        rebear
        Qt5::Core
        Qt5::Widgets
)

# Installation
install(TARGETS rebear-gui
    RUNTIME DESTINATION bin
)
