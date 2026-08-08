# RecoverySuite Common Module
add_library(RecoverySuite_Common STATIC
    dummy.cpp
)

target_include_directories(RecoverySuite_Common
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Common PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Common
    PUBLIC
        RecoverySuite_Core
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Common
        FILES_MATCHING PATTERN "*.h")