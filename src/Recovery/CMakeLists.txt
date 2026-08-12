# RecoverySuite Recovery Module
add_library(RecoverySuite_Recovery STATIC
    RecoveryCapability.h
    RecoveryCapability.cpp
    RecoverySafetyPolicy.h
    RecoverySafetyPolicy.cpp
    RecoveryValidationReport.h
    RecoveryValidationReport.cpp
    RecoveryOperationValidator.h
    RecoveryOperationValidator.cpp
    FilesystemDetector.h
    FilesystemDetector.cpp
    FilesystemAnalyzer.h
    FilesystemAnalyzer.cpp
    MetadataRecovery.h
    MetadataRecovery.cpp
    FileRecovery.h
    FileRecovery.cpp
    CarvingEngine.h
    CarvingEngine.cpp
    OutputExporter.h
    OutputExporter.cpp
)

target_include_directories(RecoverySuite_Recovery
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/Core
        ${PROJECT_BINARY_DIR}/src/Core
        ${CMAKE_SOURCE_DIR}/src/Disk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Recovery PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Recovery
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
        RecoverySuite_Logging
        RecoverySuite_Errors
        RecoverySuite_Config
        RecoverySuite_Disk
        RecoverySuite_Partition
        RecoverySuite_Filesystem
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Recovery
        FILES_MATCHING PATTERN "*.h")