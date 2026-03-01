# rebear Qt GUI application

# Enable Qt MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Source files
set(GUI_SOURCES
    # TODO: Uncomment as implemented
    # main.cpp
    # mainwindow.cpp
    # widgets/transaction_viewer.cpp
    # widgets/patch_editor.cpp
    # widgets/address_visualizer.cpp
)

# Header files
set(GUI_HEADERS
    # TODO: Uncomment as implemented
    # mainwindow.h
    # widgets/transaction_viewer.h
    # widgets/patch_editor.h
    # widgets/address_visualizer.h
)

# UI files
set(GUI_UIS
    # TODO: Uncomment as implemented
    # mainwindow.ui
)

# Create executable (commented out until sources exist)
# add_executable(rebear-gui ${GUI_SOURCES} ${GUI_HEADERS} ${GUI_UIS})

# Link against core library and Qt (commented out until executable exists)
# target_link_libraries(rebear-gui
#     PRIVATE
#         rebear
#         Qt5::Core
#         Qt5::Widgets
#         Qt5::Charts
# )

# Installation (commented out until executable exists)
# install(TARGETS rebear-gui
#     RUNTIME DESTINATION bin
# )
