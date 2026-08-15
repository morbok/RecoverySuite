#ifndef RECOVERYSUITE_GUI_CORE_APPLICATION_H
#define RECOVERYSUITE_GUI_CORE_APPLICATION_H

#include <memory>
#include <string>

namespace recoverysuite {
namespace gui {
namespace core {

/**
 * Application
 *
 * Main application class that initializes the GUI framework,
 * manages the main loop, and coordinates between UI components
 * and the service layer.
 */
class Application {
public:
    /**
     * Constructor
     */
    Application();

    /**
     * Destructor
     */
    ~Application();

    /**
     * Initialize the application
     * @return true if initialization successful, false otherwise
     */
    bool initialize();

    /**
     * Run the main application loop
     * @return application exit code
     */
    int run();

    /**
     * Shutdown the application
     */
    void shutdown();

private:
    // Implementation details
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

} // namespace core
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_CORE_APPLICATION_H