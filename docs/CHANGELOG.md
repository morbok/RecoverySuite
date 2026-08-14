# RecoverySuite Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]
    - Cross-Platform Storage and Recovery Validation (PHASE 12A): verified platform-independent Recovery logic remains separated from platform-specific storage access and validated that supported platform implementations compile correctly.
      * Verified Recovery layer contains no unnecessary platform-specific dependencies
      * Verified Linux and Windows storage abstractions follow the same interfaces where intended
      * Built the project successfully on current Linux platform
      * Added portable tests for:
        - DiskReader interface behavior
        - Filesystem detection
        - Filesystem analysis
        - Recovery validation
        - Recovery service behavior
      * Used platform-independent synthetic data wherever possible
      * Reviewed path handling for platform differences
      * Reviewed filesystem output handling for platform differences
      * Ensured platform-specific failures produce explicit errors rather than silent fallback behavior
    - Recovery Integration & Dependency Validation (Phase 10A): integrated Recovery module with actual Disk, Partition, FAT, and NTFS layers by:
      * Added RecoverySuite_Disk, RecoverySuite_Partition, and RecoverySuite_Filesystem dependencies to src/Recovery/CMakeLists.txt
      * Replaced all simulation TODOs with actual diskReader_->readSectors() calls in Recovery components
      * Fixed IDiskReader interface mismatch (changed readSectors return type from uint64_t to bool) and updated all implementations
      * Implemented missing interface methods in PhysicalDisk (open, close, getDiskInfo, getSectorSize)
      * Fixed variable shadowing issue in FileRecovery.cpp
      * Updated test mocks to match corrected IDiskReader interface
      * Verified end-to-end integration through successful build and test execution (6/6 tests passed)
    - CLI Recovery Workflow Integration (PHASE 11B): integrated application-facing Recovery service into existing CLI to provide safe workflows for listing storage sources, inspecting partitions, detecting filesystem type, analyzing sources, validating/recovery operations, selecting capabilities/output destinations, starting/cancelling operations, displaying progress/errors/completion status:
      * Created CLIHandler class with command methods for list, disk info, analyze, validate, recover, cancel, status, help, and version commands
      * Integrated with RecoveryService for operations and DiskManager for disk enumeration
      * Implemented argument parsing, validation, and confirmation prompts for destructive operations
      * Used existing RecoveryOperationValidator for pre-operation validation
      * Distinguished analysis (read-only) from recovery operations (with safety checks)
      * Added confirmation prompts for destructive operations to prevent accidental data loss
      * Prevented silent source overwriting by enforcing destination validation
      * Added clear, structured output that shows operation type and results distinctly
      * Implemented basic operation tracking for cancellation and status reporting
      * Added comprehensive test coverage using synthetic test data only
      * Updated build configuration to include new CLI source files and tests
      * All tests passing (11/11 tests)
    - Recovery Logging, Diagnostics & Audit Trail (PHASE 11C): integrated logging system with Recovery operations to provide reliable diagnostic information without leaking sensitive data.
      * Implemented thread-safe singleton Logger class with DEBUG, INFO, WARN, ERROR levels.
      * Integrated logging with RecoveryService and CLIHandler components.
      * Logged meaningful lifecycle events (initialization, analysis start/completion, recovery start/completion, cancellation, validation failure, etc.).
      * Logged useful diagnostic context (operation type, capability, filesystem type, stage, error category, safe offsets/ranges).
      * Ensured no sensitive data is logged (API keys, credentials, passwords, raw recovered file contents, unnecessary personal data).
      * Verified logging does not materially degrade recovery performance.
      * Verified failures remain diagnosable even when exceptions occur.
      * Added tests verifying important Recovery events are logged.
      * Verified logging behavior for success/failure/cancellation/validation rejection.
      * Updated build configuration to build Logger.cpp.
      * All tests passing (BasicTest, DiskTest, StorageTest, MBR partition test, GPT partition test, FAT tests, CLI tests, recovery service tests, etc.).
    - Recovery capability and safety validation (Phase 9E): implemented RecoveryCapability, RecoverySafetyPolicy, RecoveryValidationReport, RecoveryOperationValidator, FilesystemDetector, FilesystemAnalyzer, MetadataRecovery, FileRecovery, CarvingEngine, OutputExporter classes with full support for filesystem detection, analysis, metadata recovery, file recovery, carving, and output/export capabilities, including safety precondition validation and detailed error reporting.

## [0.1.0] - 2026-08-06
### Added
- Initial project setup
- Repository audit and cleanup
- Architecture documentation (6 designs)
- Build system configuration (CMake with C++20)
- Directory structure establishment
- Basic CLI executable
- Basic test framework
- Disk Layer foundation (partial implementation)
  - Disk exception handling framework
  - Device information structures
  - Disk enumerator interface
  - Disk reader interface
  - Disk device interface
  - Windows-specific disk exception
  - Windows-specific disk reader (with validation and error handling for geometry and size)
  - Windows-specific disk device
  - Updated CMake to build Disk module and conditionally build Windows-specific Disk library
  - Basic unit tests for Disk module interface (including validation and error condition tests)
### Changed
- Initial SESSION_STATE.md creation

## [0.0.1] - 2026-08-06
### Added
- Project initialization
- README.md with project title
- TEST.txt placeholder
- .gitignore for build artifacts