# Recovery Configuration Screen

This document describes the Recovery Configuration screen implemented in PHASE 14D of the RecoverySuite project.

## Overview

The Recovery Configuration screen allows users to configure recovery operations based on the capabilities registered and supported by the backend. It exposes only those capabilities that are actually available, ensuring that the GUI does not attempt to configure unsupported operations.

## Features

- **Capability Selection**: Users can select from a dropdown list of available recovery capabilities:
  - Filesystem Detection
  - Filesystem Analysis
  - Metadata Recovery
  - File Recovery
  - Carving
  - Output Export

- **Dynamic Field Configuration**: Based on the selected capability, the input fields (start sector, number of sectors, output path, etc.) are dynamically updated with appropriate placeholders and validation rules.

- **Device Information**: Displays the currently selected storage device.

- **Validation**: Includes input validation for numeric fields and required fields, with error messages displayed via QMessageBox.

- **Progress and Status**: Features a status text area that logs configuration steps and messages.

- **Integration**: The widget communicates with the MainWindow via signals to indicate when a configuration is ready for execution.

## Usage

The RecoveryConfigurationWidget is intended to be used in the RECOVERY_CONFIGURATION state of the application workflow. It is displayed in the workflow container of the MainWindow when the application state is set to RECOVERY_CONFIGURATION.

### Signals

- `configurationReady(const RecoveryOperation& operation)`: Emitted when the user validates or configures the recovery operation, passing the configured operation.

### Public Methods

- `void setRecoveryService(GUIRecoveryService* service)`: Sets the GUI recovery service for capability detection.
- `void setSelectedDevice(const QString& devicePath)`: Sets the currently selected storage device.
- `RecoveryOperation getRecoveryOperation() const`: Returns the current configuration as a RecoveryOperation model.

## Implementation Notes

- The widget uses Qt's signal and slot mechanism for UI interactions.
- Input validation is performed before emitting the configurationReady signal.
- The widget dynamically adjusts its fields based on the selected capability to guide the user towards valid configurations.
- All data presented to the user originates from actual backend results; the GUI does not invent filesystem information.

## Testing

The recovery configuration screen has been tested via:
- Manual inspection of the GUI
- Automated tests for the GUI recovery service (test_gui_recovery_service)
- Full test suite execution (ctest)

All tests pass.

## Integration with MainWindow

The MainWindow creates an instance of RecoveryConfigurationWidget and adds it to the workflow container. It connects the widget's configurationReady signal to the onConfigurationReady slot, which transitions the application to the VALIDATION state and displays a summary of the configuration.

## Screenshots

(Note: As this is a text-based document, screenshots are not included. Please refer to the running application for visual reference.)

## Future Improvements

- Integration with the actual RecoveryOperationValidator for backend validation.
- More detailed progress reporting during configuration validation.
- Support for saving and loading configuration profiles.