# RecoverySuite Configuration Module
add_library(RecoverySuite_Config STATIC
    dummy.cpp
)

target_include_directories(RecoverySuite_Config
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Config PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Config
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Config
        FILES_MATCHING PATTERN "*.h")