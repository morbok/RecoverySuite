# RecoverySuite GUI Module
cmake_minimum_required(VERSION 3.16...3.20)

# Find Qt6 packages
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Static library (without main.cpp)
add_library(RecoverySuite_GUI STATIC
    core/Application.cpp
    core/ApplicationState.cpp
    core/StateManager.cpp
    widgets/WorkflowController.cpp
    widgets/RecoveryWorkflowWidget.cpp
    widgets/OperationProgressWidget.cpp
    widgets/SourceDiscoveryWidget.cpp
    widgets/RecoveryConfigurationWidget.cpp
    core/MainWindow.cpp
)

# Headers that need moc processing
set(GUI_MOC_HEADERS
    core/MainWindow.h
    widgets/RecoveryWorkflowWidget.h
    widgets/OperationProgressWidget.h
    widgets/SourceDiscoveryWidget.h
    widgets/RecoveryConfigurationWidget.h
)

# Generate moc files
qt6_wrap_cpp(GUI_MOC_SRCS ${GUI_MOC_HEADERS})

# Add moc sources to the library
target_sources(RecoverySuite_GUI PRIVATE ${GUI_MOC_SRCS})

target_include_directories(RecoverySuite_GUI
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_GUI PRIVATE cxx_std_20)

# Link Qt libraries to the static library (make them public so executables get the includes)
target_link_libraries(RecoverySuite_GUI
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

# Executable
add_executable(RecoverySuite_GUI_Exe main.cpp)

target_link_libraries(RecoverySuite_GUI_Exe
    PRIVATE
        RecoverySuite_GUI
        RecoverySuite_Application
        RecoverySuite_Logging
        RecoverySuite_Config
        RecoverySuite_Utilities
        RecoverySuite_Common
)

# Set the output name of the executable to RecoverySuite_GUI
set_target_properties(RecoverySuite_GUI_Exe PROPERTIES OUTPUT_NAME RecoverySuite_GUI)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/GUI
        FILES_MATCHING PATTERN "*.h")

# Install the executable
install(TARGETS RecoverySuite_GUI_Exe DESTINATION bin)