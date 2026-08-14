# RecoverySuite CLI Module
add_executable(RecoverySuite_CLI
    main.cpp
    CLIHandler.cpp
)

target_include_directories(RecoverySuite_CLI
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/Core
        ${PROJECT_BINARY_DIR}/src/Core
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_CLI PRIVATE cxx_std_20)

# Link libraries
target_link_libraries(RecoverySuite_CLI
    PRIVATE
        RecoverySuite_Application
        RecoverySuite_Logging
        RecoverySuite_Config
        RecoverySuite_Utilities
        RecoverySuite_Core
        RecoverySuite_Disk
        RecoverySuite_Recovery
)

# Install executable
install(TARGETS RecoverySuite_CLI
        RUNTIME DESTINATION bin
        )