# RecoverySuite Disk Module
add_library(RecoverySuite_Disk STATIC
    DiskExceptions.h
    DeviceInformation.h
    DiskEnumerator.h
    DiskEnumerator.cpp
    DiskManager.h
    DiskManager.cpp
    Access/PhysicalDisk.h
    Access/PhysicalDisk.cpp
    IDiskReader.hpp
)

target_include_directories(RecoverySuite_Disk
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src/Core
        ${PROJECT_BINARY_DIR}/src/Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Disk PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Disk
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
        RecoverySuite_Logging
        RecoverySuite_Errors
        RecoverySuite_Config
)

# Platform-specific libraries for disk access
if(WIN32)
    # Windows-specific libraries for disk access
    target_link_libraries(RecoverySuite_Disk
        PUBLIC
            setupapi
            version
            cfgmgr32
    )
elseif(UNIX AND NOT APPLE)
    # Linux-specific libraries
    target_link_libraries(RecoverySuite_Disk
        PUBLIC
            pthread
    )
elseif(APPLE)
    # macOS-specific libraries
    target_link_libraries(RecoverySuite_Disk
        PUBLIC
            pthread
    )
endif()

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Disk
        FILES_MATCHING PATTERN "*.h")