#include "StateManager.h"
#include <iostream>

namespace recoverysuite {
namespace gui {
namespace core {

StateManager::StateManager() : currentState_(ApplicationState::START) {}

ApplicationState StateManager::getCurrentState() const {
    return currentState_;
}

bool StateManager::transitionTo(ApplicationState newState) {
    if (isValidTransition(currentState_, newState)) {
        currentState_ = newState;
        return true;
    }
    return false;
}

bool StateManager::canTransitionTo(ApplicationState newState) const {
    return isValidTransition(currentState_, newState);
}

void StateManager::forceTransitionTo(ApplicationState newState) {
    currentState_ = newState;
}

void StateManager::resetToStart() {
    currentState_ = ApplicationState::START;
}

bool StateManager::isValidTransition(ApplicationState currentState, ApplicationState newState) const {
    // Define valid transitions based on workflow requirements
    switch (currentState) {
        case ApplicationState::START:
            return newState == ApplicationState::SOURCE_SELECTION ||
                   newState == ApplicationState::EXIT;

        case ApplicationState::SOURCE_SELECTION:
            return newState == ApplicationState::STORAGE_PARTITION_INSPECTION ||
                   newState == ApplicationState::START;

        case ApplicationState::STORAGE_PARTITION_INSPECTION:
            return newState == ApplicationState::FILESYSTEM_DETECTION ||
                   newState == ApplicationState::SOURCE_SELECTION;

        case ApplicationState::FILESYSTEM_DETECTION:
            return newState == ApplicationState::FILESYSTEM_ANALYSIS ||
                   newState == ApplicationState::STORAGE_PARTITION_INSPECTION;

        case ApplicationState::FILESYSTEM_ANALYSIS:
            return newState == ApplicationState::RECOVERY_CONFIGURATION ||
                   newState == ApplicationState::FILESYSTEM_DETECTION;

        case ApplicationState::RECOVERY_CONFIGURATION:
            return newState == ApplicationState::VALIDATION ||
                   newState == ApplicationState::FILESYSTEM_ANALYSIS;

        case ApplicationState::VALIDATION:
            return newState == ApplicationState::RECOVERY ||
                   newState == ApplicationState::RECOVERY_CONFIGURATION;

        case ApplicationState::RECOVERY:
            return newState == ApplicationState::RESULTS ||
                   newState == ApplicationState::ERROR ||
                   newState == ApplicationState::VALIDATION; // Allow cancellation back to validation

        case ApplicationState::RESULTS:
            return newState == ApplicationState::SOURCE_SELECTION ||
                   newState == ApplicationState::EXIT;

        case ApplicationState::ERROR:
            return newState == ApplicationState::SOURCE_SELECTION ||
                   newState == ApplicationState::EXIT;

        case ApplicationState::EXIT:
            // Exit is terminal state - no outgoing transitions
            return false;

        default:
            return false;
    }
}

} // namespace core
} // namespace gui
} // namespace recoverysuite