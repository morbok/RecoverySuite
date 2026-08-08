# RecoverySuite Utilities Module
add_library(RecoverySuite_Utilities STATIC
    dummy.cpp
)

target_include_directories(RecoverySuite_Utilities
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
)

# Set C++20 standard for this target
target_compile_features(RecoverySuite_Utilities PRIVATE cxx_std_20)

# Link core libraries
target_link_libraries(RecoverySuite_Utilities
    PUBLIC
        RecoverySuite_Core
        RecoverySuite_Common
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION include/RecoverySuite/Utilities
        FILES_MATCHING PATTERN "*.h")