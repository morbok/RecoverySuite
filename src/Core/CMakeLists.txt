# RecoverySuite Core Module
add_library(RecoverySuite_Core STATIC
    dummy.cpp
)

# Generate config.h from config.h.in
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    )

target_include_directories(RecoverySuite_Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_BINARY_DIR}/..
        ${CMAKE_CURRENT_BINARY_DIR}
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Core PRIVATE cxx_std_20)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Core
        FILES_MATCHING PATTERN "*.h")