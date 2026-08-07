cmake_minimum_required(VERSION 3.20)
project(RecoverySuite VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Project information
set(RECOVERY_SUITE_VERSION_MAJOR 1)
set(RECOVERY_SUITE_VERSION_MINOR 0)
set(RECOVERY_SUITE_VERSION_PATCH 0)

# Compiler-specific warnings and settings
if(MSVC)
    # MSVC warnings
    add_compile_options(/W4 /Wall /permissive-)
    # Disable specific warnings that are too noisy
    add_compile_options(/wd4251 /wd4275 /wd4800)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
    # Clang warnings
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
    add_compile_options(-Wold-style-cast -Woverloaded-virtual -Wsign-promo)
    add_compile_options(-Wzero-as-null-pointer-constant -Wsuggest-override)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC warnings
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
    add_compile_options(-Wold-style-cast -Woverloaded-virtual -Wsign-promo)
    add_compile_options(-Wzero-as-null-pointer-constant -Wsuggest-override)
endif()

# Build type presets
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Install directories
set(INSTALL_INCLUDE_DIR include)
set(INSTALL_LIB_DIR lib)
set(INSTALL_BIN_DIR bin)

# Enable testing
enable_testing()

# Configure compiler detection message
message(STATUS "Building RecoverySuite ${PROJECT_VERSION}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# Create config header
configure_file(
    "${CMAKE_SOURCE_DIR}/src/Core/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/src/Core/config.h"
)

# Add source directories
add_subdirectory(src)

# Add Windows platform-specific implementations if on Windows
if(WIN32)
    add_subdirectory(src/Platform/Windows)
endif()

# Add tests
add_subdirectory(tests)

# Install headers
install(DIRECTORY include/ DESTINATION ${INSTALL_INCLUDE_DIR})