# 2026-10-31 — Bounded recursive FAT recovery

Added GUI files-with-subfolders and CLI image tree for allocated FAT12/16/32 directories/files,
including fragmented directory chains and validated Unicode names. All discovered directory chains
are reserved before file payload reads. Bound depth, directory count/read bytes, path length and
file sizes/count; replace the global cluster set with a bounded bitmap. POSIX hierarchical output
rejects symlinks/traversal, creates private folders and files, and preserves existing files.
Empty folders, original metadata, deleted chains and NTFS remain unsupported.

# 2026-10-31 — Reject directory/file cross-links

Fixed root-file recovery exporting directory clusters as file data when the directory entry appeared
later in the root. Reserve child-directory chains before file reads; added first-cluster and tail
cross-link regression images. Debug/Release: 27/27; targeted ASan/UBSan: 3/3.

# 2026-10-31 — Validated FAT long filenames

FAT12/16/32 root-file recovery now uses Unicode LFN names after validating ordinal order, checksum,
entry metadata, terminators/padding and UTF-16. Unsafe/malformed/overlong names fall back to ASCII
8.3 aliases and are counted in results. File creation remains exclusive; collisions never rename or
overwrite previous outputs. Added synthetic Unicode, record/cluster-boundary and corruption cases.

# 2026-10-31 — Allocated FAT32 root-file recovery

Extended GUI/CLI/legacy root-file recovery to FAT32: fragmented root chains, 28-bit FAT values,
full directory first-cluster fields, active-FAT selection when mirroring is disabled, and masked
comparison of mirrored cluster values. Root chains are bounded to 16 MiB and reserved against file
cross-links. Invalid root geometry/chains and unsupported FAT32 versions fail before output.
Added synthetic FAT32 fixtures covering fragmented high-numbered file clusters, dirty flags,
reserved FAT bits, active/mirrored copies, corruption and cancellation. Metadata snapshot mode
still exports boot/FAT regions only for FAT32; NTFS, deleted chains and recursive recovery remain open.

# 2026-10-31 — Allocated FAT12/16 root-file recovery

Added actual root-file copying with ASCII 8.3 aliases, fragmented allocated chains, zero-byte files,
FAT mirror validation, chain/name/resource checks and cancellation. Integrated GUI/CLI export and
legacy buffered recovery. Added FAT12/4 KiB FAT16 fixtures, dirty flags, corruption/cross-link tests,
GUI/CLI execution, no-overwrite and source preservation. Deleted entries and subdirectories remain
outside this mode.

# 2026-10-30 — Honest image boot-sector analysis

Added CLI image analyze with validated boot-sector reads and explicit unknown metrics. Removed
unaligned reads, fake version/free-space defaults and unintended full-metadata traversal; report
declared geometry warnings and sanitize boot labels for terminal output. Fixed source binding and
GUI service field conversion. NTFS boot layout reference:
https://github.com/torvalds/linux/blob/master/fs/ntfs3/ntfs.h

# 2026-10-30 — Real FAT snapshots and shared carving

Added safe GUI/CLI export of actual FAT boot sectors, FAT copies and FAT12/16 root entries. Removed
manufactured file/FAT/MFT contents. Fixed selected-range and byte-offset handling, bounded metadata
reads, short-read rejection and output inode replacement detection. Unified legacy carving with
the streaming scanner, exact absolute offsets and bounded buffered results. Added FAT12/16/32,
unaligned/4 KiB-sector, GUI/CLI, cancellation, short-read and stable-offset regressions.

Format reference: Microsoft FAT Specification, sections 3 and 6.6:
https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf

# 2026-10-30 — CLI image export

Added read-only image info/carve commands, verified filesystem export, strict flags, meaningful exit
codes and cooperative Ctrl+C/SIGTERM cancellation. Removed misleading physical-recovery completion
without saved output and fixed physical reader double ownership in analysis. Added subprocess tests.

# 2026-10-30 — Read-only image recovery and safe export

Added a real GUI source-to-output workflow for regular raw images, bounded streaming JPEG/PDF/ZIP
signature candidates, validated configuration and POSIX no-overwrite export with readback verification.
Progress/cancellation reaches scan/export loops; partial failures and completed outputs remain visible.
Added source/path/range/boundary/cancellation/GUI regressions: 24 CTest executables pass in Debug/Release,
and the image suite passes ASan/UBSan locally. Windows export and legacy recovery remain incomplete.

# 2026-10-30 — Recovery-results handoff

Completed the unfinished results view; repaired build/test registration and worker lifecycle;
removed fabricated GUI recovery metrics; fixed boot-sector and partition parser validation;
added assertion-enabled Debug/Release Linux CI. See SESSION_STATE.md for validation and limitations.

# RecoverySuite Changelog

All notable changes to this project will be documented in this file.

## [Unreleased]
    - Application-Layer Readiness Validation (PHASE 13B): verified that the backend and application-facing APIs are stable enough to support the future GUI without requiring direct access to low-level implementation details.
      * Verified the application service layer provides clean operations for source discovery, partition inspection, filesystem detection, filesystem analysis, recovery validation, recovery execution, progress, cancellation, errors, and completion
      * Verified the service layer does not duplicate engine logic
      * Verified all results are represented by stable application-facing models
      * Verified errors are structured and actionable
      * Verified long-running operations do not block the caller unnecessarily where asynchronous execution is already supported by the architecture
      * Verified cancellation works through the complete application layer
      * Verified progress information is accurate
      * Verified logging and diagnostics remain available to application clients
      * Added integration tests that exercise the service layer from start to finish using controlled data
      * Built the complete project and ran all tests
      * Updated documentation
    - Recovery Security and Safety Audit (PHASE 12B): performed a security-focused audit of the existing RecoverySuite implementation, especially around untrusted disk data, malformed filesystem structures, paths, arithmetic, memory, and output handling.
      * Audited recovery inputs as untrusted data - validated all operations begin with validator_.validateOperation() call
      * Checked for boundary condition vulnerabilities in sector range validation and arithmetic overflow protection
      * Verified proper resource release in cancellation/failure paths
      * Confirmed exception handling does not leak sensitive information
      * Added regression tests for security vulnerabilities discovered
      * Updated documentation with actual findings and fixes
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