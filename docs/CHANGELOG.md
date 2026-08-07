# RecoverySuite Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]
### Added
- Disk Layer foundation interfaces and Windows-specific implementation
  - Disk exception handling framework
  - Device information structures (DiskGeometry, DiskInfo)
  - Disk enumerator interface (IDiskEnumerator)
  - Disk reader interface (IDiskReader)
  - Disk device interface (IDiskDevice)
  - Windows-specific disk exception (WindowsDiskException)
  - Windows-specific disk reader (WindowsDiskReader)
  - Windows-specific disk device (WindowsDiskDevice)
  - Updated CMake to build Disk module and conditionally build Windows-specific Disk library
  - Basic unit tests for Disk module interface
  - Windows-specific disk tests

### Changed
- Updated SESSION_STATE.md to reflect Disk Layer foundation progress
- Updated MASTER_TODO.md to reflect actual implementation state

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