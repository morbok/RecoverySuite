#include "Application.h"
#include "Application/Service/RecoveryService.h"
#include "Logging/Logger.h"
#include <QApplication>
#include "core/MainWindow.h"

namespace recoverysuite {
namespace gui {
namespace core {

class Application::Impl {
public:
    Impl() : app_(nullptr), mainWindow_(nullptr) {}

    bool initialize() {
        try {
            // Initialize logger
            recoverysuite::logging::Logger::instance().info("GUI Application initializing");

            // Initialize QApplication (will be created in run method)
            // We don't create it here because we need to pass argc/argv

            return true;
        } catch (const std::exception& e) {
            recoverysuite::logging::Logger::instance().error("Failed to initialize GUI Application: " + std::string(e.what()));
            return false;
        }
    }

    int run() {
        try {
            // Create QApplication with dummy argc/argv since we're called from main
            static char* dummy_argv[] = { const_cast<char*>("RecoverySuite"), nullptr };
            int dummy_argc = 1;

            app_ = new QApplication(dummy_argc, dummy_argv);

            // Enable high DPI scaling
            app_->setAttribute(Qt::AA_EnableHighDpiScaling);
            app_->setAttribute(Qt::AA_UseHighDpiPixmaps);

            recoverysuite::logging::Logger::instance().info("Starting GUI Application main loop");

            // Create and show main window
            mainWindow_ = new MainWindow();
            mainWindow_->show();

            // Start the Qt event loop
            int result = app_->exec();

            // Cleanup
            delete mainWindow_;
            mainWindow_ = nullptr;

            delete app_;
            app_ = nullptr;

            recoverysuite::logging::Logger::instance().info("GUI Application main loop ended with code: " + std::to_string(result));
            return result;
        } catch (const std::exception& e) {
            recoverysuite::logging::Logger::instance().error("Error in GUI Application main loop: " + std::string(e.what()));
            return -1;
        }
    }

    void shutdown() {
        try {
            recoverysuite::logging::Logger::instance().info("Shutting down GUI Application");

            // Cleanup Qt application if it exists
            if (app_) {
                app_->quit();
                delete app_;
                app_ = nullptr;
            }

            if (mainWindow_) {
                delete mainWindow_;
                mainWindow_ = nullptr;
            }
        } catch (const std::exception& e) {
            recoverysuite::logging::Logger::instance().error("Error shutting down GUI Application: " + std::string(e.what()));
        }
    }

private:
    QApplication* app_;
    MainWindow* mainWindow_;
};

Application::Application() : pImpl_(std::make_unique<Impl>()) {}

Application::~Application() {
    shutdown();
}

bool Application::initialize() {
    return pImpl_->initialize();
}

int Application::run() {
    return pImpl_->run();
}

void Application::shutdown() {
    pImpl_->shutdown();
}

} // namespace core
} // namespace gui
} // namespace recoverysuite