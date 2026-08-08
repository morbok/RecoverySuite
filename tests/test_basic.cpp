#include <iostream>
#include "Core/Version.h"

// Simple test to verify the build system and basic functionality
int main() {
    std::cout << "Running RecoverySuite basic test..." << std::endl;

    // Verify version is accessible
    std::cout << "Version: "
              << RECOVERYSUITE_VERSION_MAJOR << "."
              << RECOVERYSUITE_VERSION_MINOR << "."
              << RECOVERYSUITE_VERSION_PATCH << std::endl;

    // Simple assertion
    if (RECOVERYSUITE_VERSION_MAJOR == 1 &&
        RECOVERYSUITE_VERSION_MINOR == 0 &&
        RECOVERYSUITE_VERSION_PATCH == 0) {
        std::cout << "Test PASSED: Version correctly set to 1.0.0" << std::endl;
        return 0;
    } else {
        std::cout << "Test FAILED: Version incorrect" << std::endl;
        return 1;
    }
}