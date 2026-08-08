# RecoverySuite Storage Module
add_library(RecoverySuite_Storage STATIC
    StorageDeviceInfo.h
    StorageExceptions.h
    StorageManager.h
    StorageManager.cpp
    Access/StorageAccess.h
    Access/StorageAccess.cpp
    Analysts/TrimAnalyst.h
    Analysts/TrimAnalyst.cpp
    Analysts/WearLevelingAnalyst.h
    Analysts/WearLevelingAnalyst.cpp
    Analysts/GarbageCollectionAnalyst.h
    Analysts/GarbageCollectionAnalyst.cpp
    Reporting/StorageReporter.h
    Reporting/StorageReporter.cpp
    Utils/StorageUtils.h
    Utils/StorageUtils.cpp
)

# Windows-specific storage access (only compile on Windows)
if(WIN32)
    target_sources(RecoverySuite_Storage PRIVATE
        Access/WINDOWS/WindowsStorageAccess.h
        Access/WINDOWS/WindowsStorageAccess.cpp
    )
endif()

target_include_directories(RecoverySuite_Storage
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Access
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils
        ${CMAKE_SOURCE_DIR}/src/Core
        ${CMAKE_SOURCE_DIR}/src/Disk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Storage PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Storage
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
        RecoverySuite_Logging
        RecoverySuite_Errors
        RecoverySuite_Config
        RecoverySuite_Disk
)

# Platform-specific libraries for storage access
if(WIN32)
    # Windows-specific libraries for storage access
    target_link_libraries(RecoverySuite_Storage
        PUBLIC
            setupapi
            version
            cfgmgr32
            wbemuuid
    )
elseif(UNIX AND NOT APPLE)
    # Linux-specific libraries
    target_link_libraries(RecoverySuite_Storage
        PUBLIC
            pthread
            /usr/lib/x86_64-linux-gnu/libudev.so.1
    )
elseif(APPLE)
    # macOS-specific libraries
    target_link_libraries(RecoverySuite_Storage
        PUBLIC
            pthread
            IOKit
            CoreFoundation
    )
endif()

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Storage
        FILES_MATCHING PATTERN "*.h")