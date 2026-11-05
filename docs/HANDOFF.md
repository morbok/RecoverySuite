# RecoverySuite handoff

Current checkout: feat/recovery-results-handoff; PR: https://github.com/morbok/RecoverySuite/pull/1.
Read PRODUCT_SCOPE.md first, then README.md, SESSION_STATE.md and MASTER_TODO.md. Older phase-completion notes are historical.

The primary product is connected-drive recovery; images are optional. New automatic scanning
separates catalog review from selected-file export and needs its own exact-head CI qualification.
Existing targeted read-only recovery backends:
- GUI and CLI signature carving for JPEG/PDF/ZIP candidates.
- FAT12/16/32 metadata snapshots: boot/FAT copies and FAT12/16 fixed root bytes.
- Allocated FAT12/16/32 root files using verified Unicode long names or ASCII 8.3 aliases and cluster chains, including
  fragmentation and zero-length files. FAT32 follows the root chain and honors the active FAT. Deleted entries and subdirectories are skipped; invalid long names fall back to ASCII aliases.
- Optional FAT directory traversal via GUI files-with-subfolders or CLI image tree, with 32-level,
  1023-directory and 64 MiB directory-read limits. Empty folders/permissions/timestamps are not exported.
- NTFS boot/initialized-MFT snapshots via GUI or CLI image mft: bounded mapping pairs and FILE
  update-sequence validation, raw bytes preserved. No user-file reconstruction or mirror fallback.
- In-use NTFS root files with resident/nonresident unnamed DATA via GUI or CLI image ntfs-files. Sequence-check
  root parent links, safe Unicode names, one name/record; no allocation-bitmap/index cross-check.
- Nonresident files: <=16 MiB/file, <=4096 runs, complete base-record mapping pairs; zero-fill
  uninitialized tails and reject MFT/prior-file overlaps. Sparse/compressed/encrypted data is unsupported.
- NTFS tree/checked modes add bounded parent paths, user-file extensions and allocation/index checks.
- Separate NTFS/FAT deleted modes export retained-record/free-cluster candidates with explicit uncertainty.
- CLI image info/analyze for geometry and FAT/NTFS boot-sector fields; unknown metrics stay unknown.

Image export uses directory-anchored POSIX/Windows writers with exclusive creation, path/name checks,
component-by-component safe directory creation for tree mode,
optional readback comparison and partial-output cleanup. Cancellation reaches source reads and writes.
Closing the GUI cancels and waits for worker cleanup; use the atomic flag, not queued cancellation.

There are 30 registered CTest executables. New integration suites cover images, CLI processes, FAT
snapshots, FAT files and NTFS MFT snapshots. Build/test commands and limits are in README.md. Check actual CI on the
current head before merge. Keep generated Graphify output and local configuration out of commits.

Legacy NTFS factory inspection now parses real boot geometry with bounded byte-wise reads;
unexamined allocation/mount/repair properties throw explicit unavailable errors. Its analyzer does
not claim whole-volume consistency.

Legacy RecoveryService recoverMetadata/recoverFiles/carveFiles return buffers, without export. The
GUI executeOperation and CLI image commands provide actual filesystem output. CarvingEngine uses
the shared scanner and absolute offsets. Never restore manufactured file/FAT/MFT contents.

Sessions/resume, native read-only CLI capture and development packages are implemented.
Exact-head CI at `8de6f93` (run `41905828702`) passed Linux Debug/Release, Windows,
fuzz, tooling and combined prototype delivery. Both OS jobs passed all 30 CTest
executables; synthetic loop/VHD acquisition and installed/extracted package smoke passed.
Newer changes require their own CI results. Do not run tests/builds/apps on the user's
workstation; use CI or a dedicated evaluation machine.

Remaining scope includes OEM short-name support, broader damaged/deleted-chain inference,
MFT bootstrap/nested attribute lists, physical-device/failing-media QA, signing,
application license/third-party notices, independent review and remaining legacy-code audit.
SMART/NVMe health, hex inspection and plugins remain outside the implemented milestone.
Signature candidates and readback equality do not establish full format or filesystem validity.

Graphify is installed as a CLI/Codex skill. Query the ignored graphify-out/graph.json first, verify
relevant sources, and run graphify update . after code changes.

New modes: image ntfs-tree (sequence-checked paths), ntfs-checked (allocation/index checks, user-file
ATTRIBUTE_LIST extensions, 64 KiB streaming <=2 GiB/file and initialized MFT <=128 MiB), ntfs-deleted
(retained unused records/free cluster candidates), fat-deleted (contiguous-free hypothesis, <=16 MiB).
Deleted confidence must remain explicit. Lost FAT first characters/LFNs and arbitrary deleted
fragmentation are not established; source and output equality do not prove original contents.
Read callback lifetimes in recoverNtfsChecked are synchronous, within item().
