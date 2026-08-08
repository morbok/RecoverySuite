# RecoverySuite Errors Module
add_library(RecoverySuite_Errors STATIC
    dummy.cpp
)

target_include_directories(RecoverySuite_Errors
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Errors PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Errors
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Errors
        FILES_MATCHING PATTERN "*.h")