# RecoverySuite Platform Module
add_library(RecoverySuite_Platform STATIC
    dummy.cpp
)

if(WIN32)
    target_sources(RecoverySuite_Platform PRIVATE
        Windows/Disk/IDiskEnumerator.h
        Windows/Disk/WindowsDiskEnumerator.h
        Windows/Disk/WindowsDiskEnumerator.cpp
    )
    target_include_directories(RecoverySuite_Platform PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Windows
        ${CMAKE_CURRENT_SOURCE_DIR}/Windows/Disk
    )
    # Windows-specific libraries
    target_link_libraries(RecoverySuite_Platform
        PUBLIC
            setupapi
            version
            cfgmgr32
        )
elseif(UNIX AND NOT APPLE)
    # Linux-specific libraries (none needed for stub)
elseif(APPLE)
    # macOS-specific libraries (none needed for stub)
endif()

target_include_directories(RecoverySuite_Platform
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Platform PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Platform
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Platform
        FILES_MATCHING PATTERN "*.h")
if(WIN32)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Windows/ DESTINATION include/RecoverySuite/Platform
            FILES_MATCHING PATTERN "*.h")
endif()