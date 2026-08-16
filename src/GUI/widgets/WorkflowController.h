#ifndef RECOVERYSUITE_GUI_WIDGETS_WORKFLOW_CONTROLLER_H
#define RECOVERYSUITE_GUI_WIDGETS_WORKFLOW_CONTROLLER_H

#include "../core/ApplicationState.h"
#include "../core/StateManager.h"
#include "../../../src/Application/Service/RecoveryService.h"

#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace recoverysuite {
namespace gui {
namespace widgets {

/**
 * WorkflowController
 *
 * Controls the GUI workflow by managing state transitions and coordinating
 * between the GUI interface and the backend service layer.
 * Implements the core RecoverySuite user workflow:
 * START → Source Selection → Storage/Partition Inspection → Filesystem Detection
 * → Filesystem Analysis → Recovery Configuration → Validation → Recovery → Results
 */
class WorkflowController {
public:
    /**
     * Constructor
     * @param recoveryService Reference to the backend recovery service
     */
    explicit WorkflowController(std::shared_ptr<recoverysuite::application::service::RecoveryService> recoveryService);

    /**
     * Destructor
     */
    ~WorkflowController();

    /**
     * Initialize the workflow controller
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * Process the current state and perform appropriate actions
     * This is the main workflow processing method
     */
    void processCurrentState();

    /**
     * Handle user action to proceed to next step in workflow
     * @return true if action was processed successfully
     */
    bool proceed();

    /**
     * Handle user action to go back to previous step in workflow
     * @return true if action was processed successfully
     */
    bool goBack();

    /**
     * Handle user action to cancel current operation
     * @return true if action was processed successfully
     */
    bool cancel();

    /**
     * Get the current application state
     * @return current state
     */
    recoverysuite::gui::core::ApplicationState getCurrentState() const;

    /**
     * Set a callback for when state changes
     * @param callback Function to call when state changes
     */
    void setStateChangeCallback(const std::function<void(recoverysuite::gui::core::ApplicationState, recoverysuite::gui::core::ApplicationState)>& callback);

    /**
     * Set a callback for when recovery operation completes
     * @param callback Function to call when recovery completes
     */
    void setRecoveryCompleteCallback(const std::function<void(bool success, const std::string& message)>& callback);

private:
    // State management
    recoverysuite::gui::core::StateManager stateManager_;
    std::shared_ptr<recoverysuite::application::service::RecoveryService> recoveryService_;

    // Callbacks
    std::function<void(recoverysuite::gui::core::ApplicationState, recoverysuite::gui::core::ApplicationState)> stateChangeCallback_;
    std::function<void(bool success, const std::string& message)> recoveryCompleteCallback_;

    // Workflow data
    std::string selectedSource_;      // Selected disk/partition identifier
    std::string detectedFilesystem_;  // Detected filesystem type
    bool validationPassed_ = false;   // Whether validation passed
    std::string recoveryError_;       // Error message from recovery operation

    // State handlers
    void handleStartState();
    void handleSourceSelectionState();
    void handleStoragePartitionInspectionState();
    void handleFilesystemDetectionState();
    void handleFilesystemAnalysisState();
    void handleRecoveryConfigurationState();
    void handleValidationState();
    void handleRecoveryState();
    void handleResultsState();
    void handleErrorState();

    // Helper methods
    void notifyStateChange(recoverysuite::gui::core::ApplicationState oldState, recoverysuite::gui::core::ApplicationState newState);
    bool validateSourceSelection();
    bool performFilesystemDetection();
    bool performFilesystemAnalysis();
    bool validateRecoveryOperation();
    bool executeRecoveryOperation();
};

} // namespace widgets
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_WIDGETS_WORKFLOW_CONTROLLER_H