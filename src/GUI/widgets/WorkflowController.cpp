#include "WorkflowController.h"
#include "../core/ApplicationState.h"
#include <iostream>

using recoverysuite::gui::core::ApplicationState;

namespace recoverysuite {
namespace gui {
namespace widgets {

WorkflowController::WorkflowController(
    std::shared_ptr<recoverysuite::application::service::RecoveryService> recoveryService)
    : recoveryService_(std::move(recoveryService)) {}

WorkflowController::~WorkflowController() = default;

bool WorkflowController::initialize() {
    // Initialize to start state
    stateManager_.resetToStart();
    return true;
}

void WorkflowController::processCurrentState() {
    switch (stateManager_.getCurrentState()) {
        case recoverysuite::gui::core::ApplicationState::START:
            handleStartState();
            break;
        case recoverysuite::gui::core::ApplicationState::SOURCE_SELECTION:
            handleSourceSelectionState();
            break;
        case recoverysuite::gui::core::ApplicationState::STORAGE_PARTITION_INSPECTION:
            handleStoragePartitionInspectionState();
            break;
        case recoverysuite::gui::core::ApplicationState::FILESYSTEM_DETECTION:
            handleFilesystemDetectionState();
            break;
        case recoverysuite::gui::core::ApplicationState::FILESYSTEM_ANALYSIS:
            handleFilesystemAnalysisState();
            break;
        case recoverysuite::gui::core::ApplicationState::RECOVERY_CONFIGURATION:
            handleRecoveryConfigurationState();
            break;
        case recoverysuite::gui::core::ApplicationState::VALIDATION:
            handleValidationState();
            break;
        case recoverysuite::gui::core::ApplicationState::RECOVERY:
            handleRecoveryState();
            break;
        case recoverysuite::gui::core::ApplicationState::RESULTS:
            handleResultsState();
            break;
        case recoverysuite::gui::core::ApplicationState::ERROR:
            handleErrorState();
            break;
        case recoverysuite::gui::core::ApplicationState::EXIT:
            // Exit state - no processing needed
            break;
    }
}

bool WorkflowController::proceed() {
    ApplicationState currentState = stateManager_.getCurrentState();
    ApplicationState nextState;

    // Determine next state based on current state
    switch (currentState) {
        case ApplicationState::START:
            nextState = ApplicationState::SOURCE_SELECTION;
            break;
        case ApplicationState::SOURCE_SELECTION:
            nextState = ApplicationState::STORAGE_PARTITION_INSPECTION;
            break;
        case ApplicationState::STORAGE_PARTITION_INSPECTION:
            nextState = ApplicationState::FILESYSTEM_DETECTION;
            break;
        case ApplicationState::FILESYSTEM_DETECTION:
            nextState = ApplicationState::FILESYSTEM_ANALYSIS;
            break;
        case ApplicationState::FILESYSTEM_ANALYSIS:
            nextState = ApplicationState::RECOVERY_CONFIGURATION;
            break;
        case ApplicationState::RECOVERY_CONFIGURATION:
            nextState = ApplicationState::VALIDATION;
            break;
        case ApplicationState::VALIDATION:
            nextState = ApplicationState::RECOVERY;
            break;
        case ApplicationState::RECOVERY:
            nextState = ApplicationState::RESULTS;
            break;
        case ApplicationState::RESULTS:
            nextState = ApplicationState::SOURCE_SELECTION; // Allow restarting workflow
            break;
        case ApplicationState::ERROR:
            nextState = ApplicationState::SOURCE_SELECTION; // Allow recovery from error
            break;
        case ApplicationState::EXIT:
            return false; // Cannot proceed from exit state
        default:
            return false; // Invalid state
    }

    // Attempt transition
    if (stateManager_.transitionTo(nextState)) {
        notifyStateChange(currentState, nextState);
        processCurrentState(); // Process the new state immediately
        return true;
    }
    return false;
}

bool WorkflowController::goBack() {
    ApplicationState currentState = stateManager_.getCurrentState();
    ApplicationState previousState;

    // Determine previous state based on current state
    switch (currentState) {
        case ApplicationState::START:
            return false; // Cannot go back from start
        case ApplicationState::SOURCE_SELECTION:
            previousState = ApplicationState::START;
            break;
        case ApplicationState::STORAGE_PARTITION_INSPECTION:
            previousState = ApplicationState::SOURCE_SELECTION;
            break;
        case ApplicationState::FILESYSTEM_DETECTION:
            previousState = ApplicationState::STORAGE_PARTITION_INSPECTION;
            break;
        case ApplicationState::FILESYSTEM_ANALYSIS:
            previousState = ApplicationState::FILESYSTEM_DETECTION;
            break;
        case ApplicationState::RECOVERY_CONFIGURATION:
            previousState = ApplicationState::FILESYSTEM_ANALYSIS;
            break;
        case ApplicationState::VALIDATION:
            previousState = ApplicationState::RECOVERY_CONFIGURATION;
            break;
        case ApplicationState::RECOVERY:
            // From recovery, going back goes to validation (not to configuration directly)
            // This allows user to adjust validation parameters
            previousState = ApplicationState::VALIDATION;
            break;
        case ApplicationState::RESULTS:
            previousState = ApplicationState::SOURCE_SELECTION; // Go back to source selection for new recovery
            break;
        case ApplicationState::ERROR:
            previousState = ApplicationState::SOURCE_SELECTION; // Go back to source selection after error
            break;
        case ApplicationState::EXIT:
            return false; // Cannot go back from exit
        default:
            return false; // Invalid state
    }

    // Attempt transition
    if (stateManager_.transitionTo(previousState)) {
        notifyStateChange(currentState, previousState);
        processCurrentState(); // Process the new state immediately
        return true;
    }
    return false;
}

bool WorkflowController::cancel() {
    ApplicationState currentState = stateManager_.getCurrentState();

    // Handle cancellation based on current state
    switch (currentState) {
        case ApplicationState::RECOVERY:
            // Cancel recovery operation - go back to validation state
            if (stateManager_.transitionTo(ApplicationState::VALIDATION)) {
                notifyStateChange(currentState, ApplicationState::VALIDATION);
                processCurrentState();
                return true;
            }
            break;
        case ApplicationState::FILESYSTEM_ANALYSIS:
            // Cancel analysis - go back to detection
            if (stateManager_.transitionTo(ApplicationState::FILESYSTEM_DETECTION)) {
                notifyStateChange(currentState, ApplicationState::FILESYSTEM_DETECTION);
                processCurrentState();
                return true;
            }
            break;
        case ApplicationState::STORAGE_PARTITION_INSPECTION:
            // Cancel inspection - go back to source selection
            if (stateManager_.transitionTo(ApplicationState::SOURCE_SELECTION)) {
                notifyStateChange(currentState, ApplicationState::SOURCE_SELECTION);
                processCurrentState();
                return true;
            }
            break;
        case ApplicationState::SOURCE_SELECTION:
            // Cancel source selection - go back to start
            if (stateManager_.transitionTo(ApplicationState::START)) {
                notifyStateChange(currentState, ApplicationState::START);
                processCurrentState();
                return true;
            }
            break;
        case ApplicationState::VALIDATION:
            // Cancel validation - go back to recovery configuration
            if (stateManager_.transitionTo(ApplicationState::RECOVERY_CONFIGURATION)) {
                notifyStateChange(currentState, ApplicationState::RECOVERY_CONFIGURATION);
                processCurrentState();
                return true;
            }
            break;
        case ApplicationState::ERROR:
            // From error, go back to source selection
            if (stateManager_.transitionTo(ApplicationState::SOURCE_SELECTION)) {
                notifyStateChange(currentState, ApplicationState::SOURCE_SELECTION);
                processCurrentState();
                return true;
            }
            break;
        case ApplicationState::RESULTS:
            // From results, go back to source selection for new workflow
            if (stateManager_.transitionTo(ApplicationState::SOURCE_SELECTION)) {
                notifyStateChange(currentState, ApplicationState::SOURCE_SELECTION);
                processCurrentState();
                return true;
            }
            break;
        default:
            // For other states, just go back normally
            return goBack();
    }
    return false;
}

ApplicationState WorkflowController::getCurrentState() const {
    return stateManager_.getCurrentState();
}

void WorkflowController::setStateChangeCallback(
    const std::function<void(ApplicationState, ApplicationState)>& callback) {
    stateChangeCallback_ = callback;
}

void WorkflowController::setRecoveryCompleteCallback(
    const std::function<void(bool success, const std::string& message)>& callback) {
    recoveryCompleteCallback_ = callback;
}

void WorkflowController::handleStartState() {
    std::cout << "[GUI] RecoverySuite Application Started" << std::endl;
    std::cout << "[GUI] Ready to begin recovery workflow" << std::endl;
}

void WorkflowController::handleSourceSelectionState() {
    std::cout << "[GUI] Source Selection Screen" << std::endl;
    std::cout << "[GUI] Please select a storage device for recovery" << std::endl;

    // In a real implementation, this would show a device selection dialog
    // and populate selectedSource_ with the user's choice
    // For now, we'll simulate selection
    selectedSource_ = "/dev/sda"; // Simulated selection
    std::cout << "[GUI] Selected source: " << selectedSource_ << std::endl;
}

void WorkflowController::handleStoragePartitionInspectionState() {
    std::cout << "[GUI] Storage/Partition Inspection" << std::endl;
    std::cout << "[GUI] inspecting partitions on " << selectedSource_ << std::endl;

    // In a real implementation, this would use the service layer to get partition info
    // For now, we'll simulate having valid partitions
    std::cout << "[GUI] Found 2 partitions:" << std::endl;
    std::cout << "[GUI]   1: NTFS, 100 GB" << std::endl;
    std::cout << "[GUI]   2: ext4, 50 GB" << std::endl;
}

void WorkflowController::handleFilesystemDetectionState() {
    std::cout << "[GUI] Filesystem Detection" << std::endl;
    std::cout << "[GUI] Detecting filesystem type on selected partition..." << std::endl;

    // In a real implementation, this would use the service layer to detect filesystem
    // For now, we'll simulate detection
    detectedFilesystem_ = "NTFS";
    std::cout << "[GUI] Detected filesystem: " << detectedFilesystem_ << std::endl;
}

void WorkflowController::handleFilesystemAnalysisState() {
    std::cout << "[GUI] Filesystem Analysis" << std::endl;
    std::cout << "[GUI] Analyzing " << detectedFilesystem_
              << " filesystem for recoverability..." << std::endl;

    // In a real implementation, this would use the service layer to analyze filesystem
    // For now, we'll simulate analysis results
    std::cout << "[GUI] Analysis complete:" << std::endl;
    std::cout << "[GUI]   Filesystem status: Healthy" << std::endl;
    std::cout << "[GUI]   MFT records: 1,250,000" << std::endl;
    std::cout << "[GUI]   Bad clusters: 0" << std::endl;
}

void WorkflowController::handleRecoveryConfigurationState() {
    std::cout << "[GUI] Recovery Configuration" << std::endl;
    std::cout << "[GUI] Configuring recovery operation for "
              << detectedFilesystem_ << " filesystem" << std::endl;
    std::cout << "[GUI] Available recovery capabilities:" << std::endl;
    std::cout << "[GUI]   1. File Recovery" << std::endl;
    std::cout << "[GUI]   2. Metadata Recovery" << std::endl;
    std::cout << "[GUI]   3. Carving" << std::endl;
    std::cout << "[GUI] Select recovery type and configure options..." << std::endl;

    // In a real implementation, this would show configuration dialogs
    // and store user selections for use in validation and recovery phases
}

void WorkflowController::handleValidationState() {
    std::cout << "[GUI] Validation" << std::endl;
    std::cout << "[GUI] Validating recovery operation before execution..." << std::endl;

    // In a real implementation, this would use the service layer to validate
    // the proposed recovery operation against safety policies
    // For now, we'll simulate validation
    bool validationResult = validateRecoveryOperation();

    if (validationResult) {
        validationPassed_ = true;
        std::cout << "[GUI] Validation PASSED - Operation is safe to proceed" << std::endl;
    } else {
        validationPassed_ = false;
        std::cout << "[GUI] Validation FAILED - Operation violates safety policies" << std::endl;
        // Automatically transition to error state on validation failure
        stateManager_.transitionTo(ApplicationState::ERROR);
        notifyStateChange(ApplicationState::VALIDATION, ApplicationState::ERROR);
        processCurrentState();
    }
}

void WorkflowController::handleRecoveryState() {
    std::cout << "[GUI] Recovery Operation" << std::endl;
    std::cout << "[GUI] Starting recovery process..." << std::endl;

    // Check if validation passed
    if (!validationPassed_) {
        std::cout << "[GUI] Recovery cannot start - validation failed" << std::endl;
        stateManager_.transitionTo(ApplicationState::ERROR);
        notifyStateChange(ApplicationState::RECOVERY, ApplicationState::ERROR);
        processCurrentState();
        return;
    }

    // In a real implementation, this would start the actual recovery operation
    // using the service layer, possibly asynchronously
    // For now, we'll simulate the recovery process
    bool recoveryResult = executeRecoveryOperation();

    if (recoveryResult) {
        std::cout << "[GUI] Recovery completed successfully!" << std::endl;
        if (recoveryCompleteCallback_) {
            recoveryCompleteCallback_(true, "Recovery operation completed successfully");
        }
    } else {
        std::cout << "[GUI] Recovery failed: " << recoveryError_ << std::endl;
        if (recoveryCompleteCallback_) {
            recoveryCompleteCallback_(false, recoveryError_);
        }
        stateManager_.transitionTo(ApplicationState::ERROR);
        notifyStateChange(ApplicationState::RECOVERY, ApplicationState::ERROR);
        processCurrentState();
    }
}

void WorkflowController::handleResultsState() {
    std::cout << "[GUI] Recovery Results" << std::endl;
    std::cout << "[GUI] Displaying recovery operation results to user" << std::endl;
    std::cout << "[GUI] User can now:" << std::endl;
    std::cout << "[GUI]   - View recovered files" << std::endl;
    std::cout << "[GUI]   - Save recovery log" << std::endl;
    std::cout << "[GUI]   - Start new recovery operation" << std::endl;
    std::cout << "[GUI]   - Exit application" << std::endl;
}

void WorkflowController::handleErrorState() {
    std::cout << "[GUI] Error State" << std::endl;
    std::cout << "[GUI] An error occurred during the recovery process" << std::endl;
    std::cout << "[GUI] User options:" << std::endl;
    std::cout << "[GUI]   - View error details" << std::endl;
    std::cout << "[GUI]   - Try again with different parameters" << std::endl;
    std::cout << "[GUI]   - Select different source device" << std::endl;
    std::cout << "[GUI]   - Exit application" << std::endl;
}

void WorkflowController::notifyStateChange(ApplicationState oldState, ApplicationState newState) {
    if (stateChangeCallback_) {
        stateChangeCallback_(oldState, newState);
    }
}

bool WorkflowController::validateSourceSelection() {
    // In a real implementation, this would validate that the selected source
    // is accessible and suitable for recovery operations
    return !selectedSource_.empty();
}

bool WorkflowController::performFilesystemDetection() {
    // In a real implementation, this would use the service layer to detect filesystem
    // For now, we'll simulate success if we have a selected source
    return !selectedSource_.empty();
}

bool WorkflowController::performFilesystemAnalysis() {
    // In a real implementation, this would use the service layer to analyze filesystem
    // For now, we'll simulate success if we have detected filesystem
    return !detectedFilesystem_.empty();
}

bool WorkflowController::validateRecoveryOperation() {
    // In a real implementation, this would use the service layer to validate
    // the recovery operation against safety policies
    // For now, we'll simulate validation based on whether we have proper state
    return !selectedSource_.empty() && !detectedFilesystem_.empty();
}

bool WorkflowController::executeRecoveryOperation() {
    // In a real implementation, this would use the service layer to execute
    // the actual recovery operation (file recovery, metadata recovery, or carving)
    // For now, we'll simulate the operation

    // Simulate potential failure conditions
    if (selectedSource_.empty()) {
        recoveryError_ = "No source selected";
        return false;
    }

    if (detectedFilesystem_.empty()) {
        recoveryError_ = "No filesystem detected";
        return false;
    }

    // Simulate successful recovery
    std::cout << "[GUI] Simulating recovery of 1245 files..." << std::endl;
    std::cout << "[GUI] Recovered 1.2 GB of data" << std::endl;

    return true;
}

} // namespace widgets
} // namespace gui
} // namespace recoverysuite