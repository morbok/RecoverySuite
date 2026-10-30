# RecoverySuite session state

Updated: 2026-10-30

## Checkpoint

Resumed from `c8475ee` (Phase 15A). The working directory contained an unfinished results view,
a truncated test CMake file, and executable permission changes across 275 tracked files.
Original work was backed up outside the repository before edits. Permissions were normalized.

The Phase 15B results presentation is now implemented and integrated. This does not mean the
complete recovery product is finished. See MASTER_TODO.md for the remaining release blockers.

## Implemented and verified locally

- Restored all previously registered test targets; registered GUI service and new QtTest results tests.
- Repaired Qt worker declarations, widget namespaces, moc integration and stale test include paths.
- Results remain visible for failed, partial and cancelled recovery operations.
- Atomic cancellation reaches the worker while its event loop is busy. Every worker outcome emits
  completion, with cleanup connections established before thread start and safe shutdown waiting.
- GUI models have initialized scalar fields. Service conversion uses actual backend data sizes and
  timestamps, removes fabricated five-second durations and scan-size-as-recovered-size statistics,
  and explicitly reports that output export has not happened.
- Unknown progress uses an indeterminate progress bar.
- Fixed filesystem range checks, validated boot-sector arithmetic, and recognized 512-byte NTFS boot sectors.
- Fixed MBR partition offset/signature byte order, GPT name bounds, declared-size CRCs, extended-entry
  bounds, entry-array resource/disk bounds, truncated sectors and boolean disk-read handling.
- Enabled assert-based tests in Release. Repaired invalid fixtures and tests that accessed private APIs.
- Added Linux Debug/Release GitHub Actions checks. Remote CI results are separate from local verification.

## Validation

Local Debug and Release configure/build and all 23 registered CTest executables pass, including real Qt widget
assertions, worker cancellation completion and malformed boot-sector/GPT input regressions.
All 23 tests also pass in Release with assertions active. Consult the PR checks for the remote CI outcome. Tests use synthetic data; no claim of
end-to-end recovery against real hardware is made.

## Next work

Implement read-only image-backed source selection, connect it to GUIRecoveryService, then implement
safe filesystem output export with no-overwrite and source/destination checks. Keep product claims
aligned with observed behavior. The existing GUI still starts with a null reader and the legacy
validation dialog contains hard-coded success checks; replace that dialog before enabling recovery.
