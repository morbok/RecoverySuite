# RecoverySuite Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]
### Added
- Storage Intelligence Subsystem implementation (Storage module)
  - StorageDeviceInfo model
  - StorageManager coordinator
  - StorageAccess layer with Windows implementation
  - TRIM, WearLeveling, and GarbageCollection analysts
  - StorageReporter for generating analysis reports
  - StorageUtils for helper functions
- Disk Layer foundation (previously implemented)
  - Disk exception handling framework
  - Device information structures
  - Disk enumerator interface
  - Disk manager class
  - Physical disk access abstraction
  - Windows-specific disk enumerator implementation

### Changed
- Updated SESSION_STATE.md to reflect Storage Intelligence subsystem completion
- Updated MASTER_TODO.md to track progress through phases

## [0.1.0] - 2026-08-06
### Added
- Initial project setup
- Repository audit and cleanup
- Architecture documentation (6 designs)
- Build system configuration (CMake with C++20)
- Directory structure establishment
- Basic CLI executable
- Basic test framework
- Disk Layer foundation
  - Disk exception handling framework
  - Device information structures
  - Disk enumerator interface
  - Disk manager class
  - Physical disk access abstraction
  - Windows-specific disk enumerator implementation
  - Unit tests for disk module interface

### Changed
- Initial SESSION_STATE.md creation

## [0.0.1] - 2026-08-06
### Added
- Project initialization
- README.md with project title
- TEST.txt placeholder
- .gitignore for build artifacts

