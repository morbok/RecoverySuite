# RecoverySuite handoff

Read README.md, SESSION_STATE.md and MASTER_TODO.md for the current supported workflow. Older phase
completion notes are historical claims and may be stale. Work is on `feat/recovery-results-handoff`,
with PR #1 at https://github.com/morbok/RecoverySuite/pull/1. Review current checks before merge.

The GUI now opens regular raw images read-only, validates the exact configured operation, streams
JPEG/PDF/ZIP signature candidates and saves their bytes with no-overwrite POSIX export. It reports
partial output failures and keeps completed files after cancellation. Closing cancels then waits for
worker cleanup. Cancellation uses an atomic flag, not a queued slot on the busy worker event loop.

Build/test commands are in README.md. There are 26 registered CTest executables; test_image_recovery
covers the source-to-output flow, hostile paths, source mutation, stream boundaries, bounded incomplete
candidates, worker cancellation and actual GUI interaction. It passes ASan/UBSan locally as well.

Do not generalize the image workflow to legacy recoverFiles/recoverMetadata/carveFiles APIs: those
still expose backend buffers/results without filesystem export. CLI image carve and GUI executeOperation connect the
new carver and safe exporter. Physical devices, fragmented/deleted-file reconstruction and Windows
safe export remain unsupported/unvalidated. Candidates can contain damaged data despite readback equality.

Graphify is installed globally as a Codex skill and CLI. Run scoped queries against the ignored
`graphify-out/graph.json`, then verify relevant source; use `graphify update .` after code edits.
Keep generated graphs, build products and local agent configuration out of commits.

CLI image info/carve now uses executeOperation; test_cli_image launches the executable and verifies exit codes, exact output, no-overwrite and SIGINT/SIGTERM cancellation.

The metadata_recovery image operation / CLI image metadata now exports bounded FAT snapshots.
Legacy recoverMetadata returns real FAT buffers (not exported); NTFS returns unavailable. Legacy
recoverFiles no longer fabricates file contents and returns unsupported. Direct contiguous byte
extraction is bounded to the selected range. CarvingEngine delegates to the shared scanner; its
carveFileType offsets are absolute byte offsets and no longer depend on previous calls.
