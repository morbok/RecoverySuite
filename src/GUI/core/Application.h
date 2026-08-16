#ifndef RECOVERYSUITE_GUI_CORE_APPLICATION_H
#define RECOVERYSUITE_GUI_CORE_APPLICATION_H

#include <memory>
#include <string>
#include "../../Application/Service/GUIRecoveryService.h"

namespace recoverysuite {
namespace gui {
namespace core {

/**
 * @brief Application
 *
 * Main application class that initializes the GUI framework,
 * manages the main loop, and coordinates between UI components
 * and the service layer.
 */
class Application {
public:
    /**
     * @brief Constructor
     */
    Application();

    /**
     * @brief Destructor
     */
    ~Application();

    /**
     * @brief Initialize the application
     * @return true if initialization successful, false otherwise
     */
    bool initialize();

    /**
     * @brief Run the main application loop
     * @return application exit code
     */
    int run();

    /**
     * @brief Shutdown the application
     */
    void shutdown();

    /**
     * @brief Get the recovery service for UI components to use
     * @return Reference to the GUI recovery service
     */
    recoverysuite::application::service::GUIRecoveryService& getRecoveryService();

private:
    // Implementation details
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace core
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_CORE_APPLICATION_H