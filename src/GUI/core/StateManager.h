#ifndef RECOVERYSUITE_GUI_CORE_STATE_MANAGER_H
#define RECOVERYSUITE_GUI_CORE_STATE_MANAGER_H

#include "ApplicationState.h"

namespace recoverysuite {
namespace gui {
namespace core {

/**
 * StateManager
 *
 * Manages application state transitions with validation to ensure
 * only valid workflow progressions are allowed.
 */
class StateManager {
public:
    /**
     * Constructor initializes to START state
     */
    StateManager();

    /**
     * Get current application state
     * @return current state
     */
    ApplicationState getCurrentState() const;

    /**
     * Attempt to transition to a new state
     * @param newState the state to transition to
     * @return true if transition successful, false if invalid
     */
    bool transitionTo(ApplicationState newState);

    /**
     * Check if a transition from current state to new state is valid
     * @param newState the state to transition to
     * @return true if transition is valid
     */
    bool canTransitionTo(ApplicationState newState) const;

    /**
     * Force transition to a state (ignores validation)
     * Used for error recovery or reset scenarios
     * @param newState the state to transition to
     */
    void forceTransitionTo(ApplicationState newState);

    /**
     * Reset to initial state
     */
    void resetToStart();

private:
    ApplicationState currentState_;

    /**
     * Validate if a transition from currentState to newState is allowed
     * @param currentState current application state
     * @param newState desired new state
     * @return true if transition is valid
     */
    bool isValidTransition(ApplicationState currentState, ApplicationState newState) const;
};

} // namespace core
} // namespace gui
} // namespace recoverysuite

#endif // RECOVERYSUITE_GUI_CORE_STATE_MANAGER_H