#include "Application.h"
#include "Application/Service/GUIRecoveryService.h"
#include "Logging/Logger.h"
#include <QApplication>
#include <QCoreApplication>
#include "core/MainWindow.h"
#include <memory>

namespace recoverysuite {
namespace gui {
namespace core {

class Application::Impl {
public:
    Impl() : app_(nullptr), mainWindow_(nullptr), guiRecoveryService_(nullptr) {}

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
            // Use the existing QApplication instance (created in main)
            app_ = qobject_cast<QApplication*>(QCoreApplication::instance());
            if (!app_) {
                recoverysuite::logging::Logger::instance().error("QApplication instance not found. Cannot start GUI application.");
                return -1;
            }

            // Enable high DPI scaling (if not already set by main)
            app_->setAttribute(Qt::AA_EnableHighDpiScaling);
            app_->setAttribute(Qt::AA_UseHighDpiPixmaps);

            recoverysuite::logging::Logger::instance().info("Starting GUI Application main loop");

            // TODO: In a real implementation, we would get a proper disk reader from the disk manager
            // For now, we'll note that the GUI recovery service needs a proper disk reader
            // This would be obtained from the DiskManager or similar component in the disk layer
            // auto diskReader = DiskManager::getInstance()->getDefaultDiskReader(); // Example
            // guiRecoveryService_ = std::make_unique<GUIRecoveryService>(diskReader);

            // For now, we'll create the service with a null disk reader - it will handle errors gracefully
            guiRecoveryService_ = std::make_unique<recoverysuite::application::service::GUIRecoveryService>(nullptr);

            // Create and show main window, passing the recovery service
            mainWindow_ = new MainWindow(guiRecoveryService_.get());
            mainWindow_->show();

            // Start the Qt event loop
            int result = app_->exec();

            // Cleanup
            delete mainWindow_;
            mainWindow_ = nullptr;

            guiRecoveryService_.reset();

            // Note: We do not delete app_ because we do not own the QApplication instance
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

            // Cleanup Qt application if it exists (we don't own it, so just quit)
            if (app_) {
                app_->quit();
                app_ = nullptr;
            }

            if (mainWindow_) {
                delete mainWindow_;
                mainWindow_ = nullptr;
            }

            if (guiRecoveryService_) {
                guiRecoveryService_.reset();
            }
        } catch (const std::exception& e) {
            recoverysuite::logging::Logger::instance().error("Error shutting down GUI Application: " + std::string(e.what()));
        }
    }

    /**
     * @brief Get the recovery service for UI components to use
     * @return Reference to the GUI recovery service
     */
    recoverysuite::application::service::GUIRecoveryService& getRecoveryService() {
        if (guiRecoveryService_) {
            return *guiRecoveryService_;
        }
        // Return a dummy service if not initialized (should not happen in normal operation)
        static recoverysuite::application::service::GUIRecoveryService dummyService(nullptr);
        return dummyService;
    }

private:
    QApplication* app_;
    MainWindow* mainWindow_;
    std::unique_ptr<recoverysuite::application::service::GUIRecoveryService> guiRecoveryService_;
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

recoverysuite::application::service::GUIRecoveryService& Application::getRecoveryService() {
    return pImpl_->getRecoveryService();
}

} // namespace core
} // namespace gui
} // namespace recoverysuite