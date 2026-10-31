# RecoverySuite active roadmap

Updated: 2026-10-31. Historical phase summaries are not the release checklist.

## Completed in the handoff

- [x] Recover Claude's unfinished results interface and restore a buildable Linux checkout.
- [x] Show completed, failed, cancelled and partial results with backend item names/sizes.
- [x] Remove fabricated duration/recovered-byte statistics and distinguish requested output from saved output.
- [x] Restore and run the 28 registered CTest targets with assertions enabled.
- [x] Repair the input-validation and partition parsing bugs uncovered by active assertions.
- [x] Add Debug/Release Linux CI and reproducible build instructions.

## Release blockers, in order

1. [x] Add a read-only disk-image reader and source selection with synthetic image integration tests.
2. [x] Bind opened images to GUIRecoveryService and reject operations on another source.
3. [x] Execute the configured carving operation and replace hard-coded validation with actual checks.
       [x] Add verified CLI export and bounded FAT metadata snapshots.
       [x] Copy allocated FAT12/16/32 root files with 8.3 aliases and fragmented cluster chains.
       [x] Decode validated Unicode long names with safe ASCII alias fallback.
       [x] Add bounded recursive FAT directory/file recovery and safe hierarchical POSIX output.
       [x] Add bounded NTFS initialized-MFT snapshots with mapping-pairs and FILE fixup checks.
       [x] Copy in-use NTFS root files with resident unnamed DATA and sequence-checked parent names.
       [x] Copy bounded nonresident NTFS root-file DATA, including fragmented runs and zero-filled uninitialized tails.
       [x] Add bounded NTFS directory traversal and user-file attribute-list extensions.
       [x] Check MFT/cluster allocation and parent directory index membership in checked mode.
       [x] Stream checked NTFS files <=2 GiB and initialized MFT <=128 MiB with bounded buffers.
       [x] Add explicit FAT contiguous-free and NTFS retained-record deleted candidates with confidence reporting.
       [ ] Add OEM short names, MFT bootstrap/nested attribute-list extensions and broader damaged/deleted-chain inference.
4. [x] Export signature candidates safely on POSIX, preserving bytes and rejecting traversal, symlinks,
       overwrite and invalid destinations; verify saved bytes in integration tests.
5. [x] Propagate cancellation/progress into image scan/export loops and test worker shutdown.
       [x] Add cancellation to FAT metadata snapshot reads and reuse the scanner for legacy carving.
       [ ] Extend cancellation to other filesystem analysis loops.
6. [ ] Audit remaining simulated recovery implementations, FAT/NTFS completeness, boundary arithmetic,
       short reads, hostile metadata, memory limits and error propagation. Register any useful orphaned tests.
       [x] Correct legacy NTFS boot fields and remove fabricated consistency/allocation/repair metrics.
7. [ ] Add end-to-end fixtures for deleted files, fragmentation, corruption and partial output failures;
       run sanitizers and fuzz parsers before real-device testing.
8. [ ] Complete native platform validation (real hardware remains open).
       [x] Replace fake inventory with Linux sysfs / Windows storage metadata.
       [x] Add guarded read-only physical acquisition, source/destination checks and safe Windows output.
       [x] Add Windows build/tests and Linux synthetic loop acquisition CI.
       [ ] Verify Windows device acquisition and physical USB/SATA/NVMe behavior.
9. [x] Implement session persistence and resume with versioned state and source identity checks.
10. [ ] Complete intended SSD diagnostics, hex inspection and optional plugin support according to actual
        product scope; expose unavailable features honestly.
11. [ ] Finish stable-release QA and redistribution decisions.
        [x] Add development Linux TGZ/Ubuntu DEB and Windows ZIP with checksums.
        [x] Add installed/extracted CLI and GUI smoke tests and instrumented parser fuzzing.
        [ ] Complete hardware matrix, signing, application license and third-party notices, and code review.

## Git workflow

Work on feature branches, make logical Conventional Commits, push, open a PR, and review before merging.
Do not commit build outputs or local agent configuration. Do not mark the product finished based on
phase descriptions or passing unit tests alone.
