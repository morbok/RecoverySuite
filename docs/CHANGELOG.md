# RecoverySuite Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]
### Added
- Disk Layer foundation validation and testing improvements
  - WindowsDiskReader bounds checking for read requests
  - WindowsDiskReader proper error handling on open failures (close handle before throwing)
  - Expanded test suite for Disk module to validate validation and error conditions
### Changed
- Updated SESSION_STATE.md to reflect Disk Layer foundation progress
- Updated MASTER_TODO.md to reflect actual implementation state
- Updated CHANGELOG.md to reflect the current changes

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