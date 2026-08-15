#include "core/Application.h"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        recoverysuite::gui::core::Application app;

        if (!app.initialize()) {
            std::cerr << "Failed to initialize RecoverySuite GUI Application" << std::endl;
            return 1;
        }

        int result = app.run();

        return result;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in RecoverySuite GUI Application: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown unhandled exception in RecoverySuite GUI Application" << std::endl;
        return 1;
    }
}