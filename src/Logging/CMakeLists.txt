# RecoverySuite Logging Module
add_library(RecoverySuite_Logging STATIC
    Logger.cpp
)

target_include_directories(RecoverySuite_Logging
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Logging PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Logging
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Logging
        FILES_MATCHING PATTERN "*.h")