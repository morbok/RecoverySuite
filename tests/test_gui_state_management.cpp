#include <iostream>
#include "../../src/GUI/core/ApplicationState.h"
#include "../../src/GUI/core/StateManager.h"

using namespace recoverysuite::gui::core;

/**
 * Simple test for GUI state management functionality without external dependencies
 */
int main() {
    bool allTestsPassed = true;

    // Test 1: Basic state transitions
    {
        StateManager stateManager;

        // Initial state should be START
        if (stateManager.getCurrentState() != ApplicationState::START) {
            std::cerr << "FAIL: Initial state should be START" << std::endl;
            allTestsPassed = false;
        }

        // Should be able to transition to SOURCE_SELECTION
        if (!stateManager.transitionTo(ApplicationState::SOURCE_SELECTION)) {
            std::cerr << "FAIL: Should be able to transition to SOURCE_SELECTION" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.getCurrentState() != ApplicationState::SOURCE_SELECTION) {
            std::cerr << "FAIL: Current state should be SOURCE_SELECTION" << std::endl;
            allTestsPassed = false;
        }

        // Should be able to transition to STORAGE_PARTITION_INSPECTION
        if (!stateManager.transitionTo(ApplicationState::STORAGE_PARTITION_INSPECTION)) {
            std::cerr << "FAIL: Should be able to transition to STORAGE_PARTITION_INSPECTION" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.getCurrentState() != ApplicationState::STORAGE_PARTITION_INSPECTION) {
            std::cerr << "FAIL: Current state should be STORAGE_PARTITION_INSPECTION" << std::endl;
            allTestsPassed = false;
        }

        std::cout << "PASS: Basic state transitions" << std::endl;
    }

    // Test 2: Invalid transitions
    {
        StateManager stateManager;

        // Start state - should not be able to go directly to FILESYSTEM_DETECTION
        if (stateManager.transitionTo(ApplicationState::FILESYSTEM_DETECTION)) {
            std::cerr << "FAIL: Should not be able to go directly to FILESYSTEM_DETECTION from START" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.getCurrentState() != ApplicationState::START) {
            std::cerr << "FAIL: State should remain START after invalid transition" << std::endl;
            allTestsPassed = false;
        }

        // Should be able to go to SOURCE_SELECTION first
        if (!stateManager.transitionTo(ApplicationState::SOURCE_SELECTION)) {
            std::cerr << "FAIL: Should be able to go to SOURCE_SELECTION" << std::endl;
            allTestsPassed = false;
        }

        // From SOURCE_SELECTION, should not be able to go to FILESYSTEM_ANALYSIS directly
        if (stateManager.transitionTo(ApplicationState::FILESYSTEM_ANALYSIS)) {
            std::cerr << "FAIL: Should not be able to go directly to FILESYSTEM_ANALYSIS from SOURCE_SELECTION" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.getCurrentState() != ApplicationState::SOURCE_SELECTION) {
            std::cerr << "FAIL: State should remain SOURCE_SELECTION after invalid transition" << std::endl;
            allTestsPassed = false;
        }

        // Should be able to go to STORAGE_PARTITION_INSPECTION
        if (!stateManager.transitionTo(ApplicationState::STORAGE_PARTITION_INSPECTION)) {
            std::cerr << "FAIL: Should be able to go to STORAGE_PARTITION_INSPECTION" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.getCurrentState() != ApplicationState::STORAGE_PARTITION_INSPECTION) {
            std::cerr << "FAIL: Current state should be STORAGE_PARTITION_INSPECTION" << std::endl;
            allTestsPassed = false;
        }

        // From STORAGE_PARTITION_INSPECTION, should be able to go to FILESYSTEM_DETECTION
        if (!stateManager.transitionTo(ApplicationState::FILESYSTEM_DETECTION)) {
            std::cerr << "FAIL: Should be able to go to FILESYSTEM_DETECTION" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.getCurrentState() != ApplicationState::FILESYSTEM_DETECTION) {
            std::cerr << "FAIL: Current state should be FILESYSTEM_DETECTION" << std::endl;
            allTestsPassed = false;
        }

        std::cout << "PASS: Invalid transitions properly rejected" << std::endl;
    }

    // Test 3: canTransitionTo method
    {
        StateManager stateManager;

        // Test canTransitionTo method
        if (!stateManager.canTransitionTo(ApplicationState::SOURCE_SELECTION)) {
            std::cerr << "FAIL: Should be able to transition to SOURCE_SELECTION from START" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.canTransitionTo(ApplicationState::FILESYSTEM_ANALYSIS)) {
            std::cerr << "FAIL: Should not be able to transition to FILESYSTEM_ANALYSIS from START" << std::endl;
            allTestsPassed = false;
        }

        // Navigate through workflow
        stateManager.transitionTo(ApplicationState::SOURCE_SELECTION);
        if (!stateManager.canTransitionTo(ApplicationState::STORAGE_PARTITION_INSPECTION)) {
            std::cerr << "FAIL: Should be able to transition to STORAGE_PARTITION_INSPECTION from SOURCE_SELECTION" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.canTransitionTo(ApplicationState::RECOVERY)) {
            std::cerr << "FAIL: Should not be able to transition to RECOVERY from SOURCE_SELECTION" << std::endl;
            allTestsPassed = false;
        }

        stateManager.transitionTo(ApplicationState::STORAGE_PARTITION_INSPECTION);
        if (!stateManager.canTransitionTo(ApplicationState::FILESYSTEM_DETECTION)) {
            std::cerr << "FAIL: Should be able to transition to FILESYSTEM_DETECTION from STORAGE_PARTITION_INSPECTION" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.canTransitionTo(ApplicationState::VALIDATION)) {
            std::cerr << "FAIL: Should not be able to transition to VALIDATION from STORAGE_PARTITION_INSPECTION" << std::endl;
            allTestsPassed = false;
        }

        std::cout << "PASS: canTransitionTo method works correctly" << std::endl;
    }

    // Test 4: Cancellation and error handling
    {
        StateManager stateManager;

        // Navigate to recovery state through valid path
        stateManager.transitionTo(ApplicationState::SOURCE_SELECTION);
        stateManager.transitionTo(ApplicationState::STORAGE_PARTITION_INSPECTION);
        stateManager.transitionTo(ApplicationState::FILESYSTEM_DETECTION);
        stateManager.transitionTo(ApplicationState::FILESYSTEM_ANALYSIS);
        stateManager.transitionTo(ApplicationState::RECOVERY_CONFIGURATION);
        stateManager.transitionTo(ApplicationState::VALIDATION);
        stateManager.transitionTo(ApplicationState::RECOVERY);

        if (stateManager.getCurrentState() != ApplicationState::RECOVERY) {
            std::cerr << "FAIL: Should be in RECOVERY state" << std::endl;
            allTestsPassed = false;
        }

        // From RECOVERY, should be able to go to ERROR (simulating failure)
        if (!stateManager.transitionTo(ApplicationState::ERROR)) {
            std::cerr << "FAIL: Should be able to transition to ERROR from RECOVERY" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.getCurrentState() != ApplicationState::ERROR) {
            std::cerr << "FAIL: Current state should be ERROR" << std::endl;
            allTestsPassed = false;
        }

        // From ERROR, should be able to go back to SOURCE_SELECTION
        if (!stateManager.transitionTo(ApplicationState::SOURCE_SELECTION)) {
            std::cerr << "FAIL: Should be able to go back to SOURCE_SELECTION from ERROR" << std::endl;
            allTestsPassed = false;
        }
        if (stateManager.getCurrentState() != ApplicationState::SOURCE_SELECTION) {
            std::cerr << "FAIL: Current state should be SOURCE_SELECTION" << std::endl;
            allTestsPassed = false;
        }

        // Test force transition for error recovery scenarios
        stateManager.forceTransitionTo(ApplicationState::START);
        if (stateManager.getCurrentState() != ApplicationState::START) {
            std::cerr << "FAIL: Force transition to START failed" << std::endl;
            allTestsPassed = false;
        }

        std::cout << "PASS: Cancellation and error handling works correctly" << std::endl;
    }

    if (allTestsPassed) {
        std::cout << "=== ALL GUI STATE MANAGEMENT TESTS PASSED ===" << std::endl;
        return 0;
    } else {
        std::cerr << "=== SOME GUI STATE MANAGEMENT TESTS FAILED ===" << std::endl;
        return 1;
    }
}