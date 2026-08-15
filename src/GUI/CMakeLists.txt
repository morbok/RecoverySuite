# RecoverySuite GUI Module
add_library(RecoverySuite_GUI STATIC
    main.cpp
    core/Application.cpp
    core/ApplicationState.cpp
    core/StateManager.cpp
    widgets/WorkflowController.cpp
)

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

# Link libraries
target_link_libraries(RecoverySuite_GUI
    PRIVATE
        RecoverySuite_Application
        RecoverySuite_Logging
        RecoverySuite_Config
        RecoverySuite_Utilities
        RecoverySuite_Common
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/GUI
        FILES_MATCHING PATTERN "*.h")
