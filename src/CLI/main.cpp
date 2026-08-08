#include <iostream>
#include "../src/Core/Version.h"

int main(int argc, char* argv[]) {
    std::cout << "RecoverySuite Enterprise Data Recovery Platform" << std::endl;
    std::cout << "Version: " << RECOVERYSUITE_VERSION_MAJOR << "."
              << RECOVERYSUITE_VERSION_MINOR << "."
              << RECOVERYSUITE_VERSION_PATCH << std::endl;
    std::cout << "Build system initialized successfully!" << std::endl;
    return 0;
}