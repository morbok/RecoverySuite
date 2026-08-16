# RecoverySuite Application Module
add_library(RecoverySuite_Application STATIC
    Service/RecoveryService.cpp
    Service/GUIRecoveryService.cpp
)

target_include_directories(RecoverySuite_Application
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Application PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Application
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
        RecoverySuite_Logging
        RecoverySuite_Config
        RecoverySuite_Errors
        RecoverySuite_Recovery
        RecoverySuite_Disk
        RecoverySuite_Filesystem
        RecoverySuite_Partition
        RecoverySuite_Storage
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Application
        FILES_MATCHING PATTERN "*.h")