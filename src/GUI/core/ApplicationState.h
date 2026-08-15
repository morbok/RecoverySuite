#ifndef RECOVERYSUITE_GUI_CORE_APPLICATION_STATE_H
#define RECOVERYSUITE_GUI_CORE_APPLICATION_STATE_H

namespace recoverysuite {
namespace gui {
namespace core {

/**
 * ApplicationState
 *
 * Explicit enumeration of all possible states in the RecoverySuite GUI workflow.
 * This ensures navigation state is explicit and invalid transitions can be rejected.
 */
enum class ApplicationState {
    START,                    ///< Initial application state
    SOURCE_SELECTION,         ///< User selecting a recovery source (disk/partition)
    STORAGE_PARTITION_INSPECTION, ///< Inspecting storage and partition details
    FILESYSTEM_DETECTION,     ///< Detecting filesystem type on selected source
    FILESYSTEM_ANALYSIS,      ///< Analyzing filesystem structure and health
    RECOVERY_CONFIGURATION,   ///< Configuring recovery operation parameters
    VALIDATION,               ///< Validating recovery operation before execution
    RECOVERY,                 ///< Active recovery operation in progress
    RESULTS,                  ///< Displaying recovery results
    ERROR,                    ///< Error state - displays error information
    EXIT                      ///< Application exit state
};

} // namespace core
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_CORE_APPLICATION_STATE_H