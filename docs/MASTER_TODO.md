# RecoverySuite active roadmap

Updated: 2026-10-30. Historical phase summaries are not the release checklist.

## Completed in the handoff

- [x] Recover Claude's unfinished results interface and restore a buildable Linux checkout.
- [x] Show completed, failed, cancelled and partial results with backend item names/sizes.
- [x] Remove fabricated duration/recovered-byte statistics and distinguish requested output from saved output.
- [x] Restore and run the 23 registered CTest targets with assertions enabled.
- [x] Repair the input-validation and partition parsing bugs uncovered by active assertions.
- [x] Add Debug/Release Linux CI and reproducible build instructions.

## Release blockers, in order

1. [ ] Add a read-only disk-image reader and source selection with synthetic image integration tests.
2. [ ] Wire actual source selection/discovery into GUIRecoveryService (startup currently passes nullptr).
3. [ ] Use the configured operation in execution, dispatch file/metadata/carving correctly, and replace
       the hard-coded validation-success dialog with real validation results.
4. [ ] Implement output export to regular files, preserving bytes and safe names, rejecting traversal,
       symlinks, overwrite and source/destination conflicts; verify exported contents in tests.
5. [ ] Propagate cancellation/progress into backend scan loops; remove UI-thread work scheduled through
       QTimer and test shutdown/cancellation during an actual synthetic scan.
6. [ ] Audit remaining simulated recovery implementations, FAT/NTFS completeness, boundary arithmetic,
       short reads, hostile metadata, memory limits and error propagation. Register any useful orphaned tests.
7. [ ] Add end-to-end fixtures for deleted files, fragmentation, corruption and partial output failures;
       run sanitizers and fuzz parsers before real-device testing.
8. [ ] Validate Windows builds/device access and Linux device enumeration; make permissions and errors explicit.
9. [ ] Implement session persistence and resume with versioned state and source identity checks.
10. [ ] Complete intended SSD diagnostics, hex inspection and optional plugin support according to actual
        product scope; expose unavailable features honestly.
11. [ ] Package installers, document supported recovery capabilities and limitations, and complete release QA.

## Git workflow

Work on feature branches, make logical Conventional Commits, push, open a PR, and review before merging.
Do not commit build outputs or local agent configuration. Do not mark the product finished based on
phase descriptions or passing unit tests alone.
