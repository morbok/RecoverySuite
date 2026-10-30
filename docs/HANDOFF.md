# RecoverySuite handoff

Current checkout: feat/recovery-results-handoff; PR: https://github.com/morbok/RecoverySuite/pull/1.
Read README.md, SESSION_STATE.md and MASTER_TODO.md. Older phase-completion notes are historical.

Supported read-only image workflows:
- GUI and CLI signature carving for JPEG/PDF/ZIP candidates.
- FAT12/16/32 metadata snapshots: boot/FAT copies and FAT12/16 fixed root bytes.
- Allocated FAT12/16/32 root files using ASCII 8.3 aliases and verified cluster chains, including
  fragmentation and zero-length files. FAT32 follows the root chain and honors the active FAT. Deleted entries, LFN records and subdirectories are skipped.
- CLI image info/analyze for geometry and FAT/NTFS boot-sector fields; unknown metrics stay unknown.

All image export uses a directory-anchored POSIX writer with exclusive creation, path/name checks,
optional readback comparison and partial-output cleanup. Cancellation reaches source reads and writes.
Closing the GUI cancels and waits for worker cleanup; use the atomic flag, not queued cancellation.

There are 27 registered CTest executables. New integration suites cover images, CLI processes, FAT
snapshots and FAT root files. Build/test commands and limits are in README.md. Check actual CI on the
current head before merge. Keep generated Graphify output and local configuration out of commits.

Legacy RecoveryService recoverMetadata/recoverFiles/carveFiles return buffers, without export. The
GUI executeOperation and CLI image commands provide actual filesystem output. CarvingEngine uses
the shared scanner and absolute offsets. Never restore manufactured file/FAT/MFT contents.

Remaining scope includes deleted-file chain inference, recursive directories/long names,
NTFS MFT/files, Windows safe export, session resume, hardware QA and packaging. Signature
candidates and readback equality do not establish full format or filesystem validity.

Graphify is installed as a CLI/Codex skill. Query the ignored graphify-out/graph.json first, verify
relevant sources, and run graphify update . after code changes.
