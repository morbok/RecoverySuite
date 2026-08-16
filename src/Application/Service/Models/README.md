# GUI-Facing Models

This directory contains the stable, application-facing models designed for GUI consumption.
These models provide a clean abstraction boundary between the GUI layer and the backend
recovery operations, ensuring the GUI does not depend on backend implementation details.

## Models Overview

### StorageDevice
Represents a storage device (disk) that can be analyzed or used for recovery operations.
- Provides essential identification and capacity information
- Abstracts away low-level device handling details
- Contains enough information for device selection in a GUI

### Partition
Represents a partition on a storage device.
- Provides filesystem type, size, usage information
- Contains bootability and recognition status
- Suitable for displaying partition maps in a GUI

### FilesystemInfo
Represents detailed information about a filesystem.
- Includes type, version, capacity, usage statistics
- Contains metadata like volume label and serial number
- Suitable for filesystem properties dialogs

### AnalysisResult
Result of a filesystem analysis operation.
- Contains success status and error information
- Includes basic filesystem information
- Contains detailed analysis results for advanced users
- Timestamp for tracking when analysis was performed

### RecoveryOperation
Parameters for initiating a recovery operation.
- Specifies operation type (file recovery, metadata recovery, carving)
- Identifies target device and sector range
- Specifies output destination
- Includes verification and logging options

### RecoveryProgress
Progress information for ongoing recovery operations.
- Unique operation ID for tracking
- Operation type and current status description
- Percentage completion (0-100)
- Current step description
- Processed/total bytes and items
- Timing information (elapsed, estimated remaining)
- Cancellation and pausing capability flags
- Timestamp for when progress was last updated

### RecoveryResult
Result of a completed recovery operation.
- Success status and error information
- Operation type and timing information
- Processed/recovered/failed bytes and items counts
- Additional details map for extensibility

### RecoveredItem
Represents a single recovered file or metadata item.
- File name and original path
- File size and detected type
- Location information (start/end sectors)
- Validation status and information
- Timestamps (creation/modification) if available

### ErrorInfo
Structured error information for GUI display.
- Error code for programmatic handling
- Human-readable message and detailed technical message
- Recovery suggestion for user guidance
- Recoverability flag
- Timestamp

### DiagnosticEvent
Structured diagnostic/log events for GUI display.
- Event ID and category (info/warning/error)
- Message and source component
- Timestamp
- Additional details map

## Design Principles

1. **Clean Abstraction**: Models contain only information needed by the GUI
2. **No Backend Dependencies**: Models do not expose backend implementation classes
3. **Sufficient Information**: Each model contains enough data for a professional desktop interface
4. **Extensibility**: Use of maps/dictionaries allows for future extension without breaking changes
5. **Value Semantics**: All models are plain structs with value semantics (no pointers/references)
6. **Clear Ownership**: Models are passed by value or const reference, clear ownership semantics

## Usage Example

The GUI would typically:
1. Call `enumerateStorageDevices()` to get available disks
2. For selected disk, call `getDevicePartitions()` to show partitions
3. For selected partition, call `analyzeFilesystem()` to get filesystem details
4. Based on analysis, user selects recovery operation type and parameters
5. GUI calls appropriate recovery method with progress and cancellation callbacks
6. During operation, GUI receives progress updates via callback
7. After operation, GUI receives result and displays recovered items
8. Errors and diagnostics are displayed using the ErrorInfo and DiagnosticEvent models

## Backend Independence

These models are designed to be completely independent of:
- PhysicalDisk class and its internals
- FAT/NTFS/extras filesystem implementation details
- Low-level partition parsers (MBR/GPT/etc.)
- Internal Recovery implementation classes
- Threading/concurrency implementation details
- Specific error handling mechanisms

The GUI layer only needs to include `GUIModels.h` and `GUIRecoveryService.h` to interact
with the recovery functionality, ensuring a clean separation of concerns.