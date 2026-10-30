# RecoverySuite session state

Updated: 2026-10-30

## Checkpoint

Resumed Claude's Phase 15A work at `c8475ee`, restored the results interface, repaired Qt/build/test
integration and partition/input validation, and added Linux Debug/Release CI. Original local work
was backed up outside the repository before edits; unintended executable bits were normalized.

The next checkpoint implements the first usable image-to-output GUI flow. It does not complete
the recovery product. README.md describes the supported workflow and MASTER_TODO.md tracks blockers.

## Implemented

- Read-only regular image reader, sector-size/range limits and POSIX identity/size/mtime checks.
- Actual image selection and operation configuration; no fake discovery or validation success dialog.
- Bounded streaming JPEG/PDF/ZIP candidate carving, including nonzero start sectors and signatures
  split across read boundaries. Missing end markers and candidates above 16 MiB are reported.
- POSIX directory-anchored, no-overwrite export; traversal/symlink/name rejection, readback verification,
  partial-write cleanup, per-output failures and preservation of completed files after cancellation.
- Worker-thread execution uses the exact configured operation; progress/cancellation reaches scan and
  export loops. Source changes/concurrent operations are blocked and closing waits for worker cleanup.
- Results show actual saved names, bytes, failures, cancellation and measured timing.

## Validation

26 CTest executables pass locally in Debug and Release with assertions active. The image integration
suite also passes AddressSanitizer/UBSan. Tests cover source mutation, invalid ranges/destinations,
symlinks/hardlinks/traversal, no-overwrite/partial output, cancellation, incomplete/oversized candidates,
GUI clicks and closing during recovery. Source hashes and saved bytes are checked on synthetic images.
Remote CI must be checked for the current PR head before merge.

## Limits and next work

Only the new image GUI workflow exports files. Legacy file/metadata recovery, raw-device
discovery, FAT/NTFS completeness, fragmentation, resumable sessions and Windows export remain work.
The signature carver does not validate full file formats. Limits: 16 MiB/candidate, 10,000 attempted
outputs, 64 KiB scan chunks. Windows and real hardware have not been validated.

Graphify 0.9.97 was installed as a local developer tool and Codex skill. The ignored graphify-out/
contains the AST graph, report, HTML and query memory. Use scoped queries first and update after edits;
inferred call edges require source verification. No new runtime dependency was added to the product.

## CLI checkpoint

Added image info/carve commands using the same read-only source, validated range and safe export as
the GUI. Added foreground SIGINT/SIGTERM cancellation and meaningful exit codes. Removed legacy
physical-recovery success claims without export, and fixed double ownership of physical disk readers
in analysis. A real-process QtTest suite checks saved bytes, parser failures, no-overwrite and signals.

## FAT snapshot and backend audit checkpoint

Added verified FAT12/16/32 metadata snapshots to GUI and CLI, including nonzero volume offsets,
512/1024/4096-byte sectors and no-overwrite export. Snapshot scope is explicit: boot/FAT copies plus
FAT12/16 fixed root entries; no FAT32 root chain, NTFS MFT or metadata repair. Each region is capped
at 16 MiB. Cancellation reaches each 64 KiB read and output loop.

Removed manufactured README.TXT/BINARY.BIN/FAT/MFT contents from legacy recovery. Directory/cluster
reconstruction now fails explicitly. Corrected contiguous-file and metadata byte extraction for
actual sector sizes, unaligned offsets, selected-range containment, overflow and short/oversized reads.
Legacy carving now shares the bounded streaming implementation and reports stable absolute offsets;
its buffered APIs are capped at 64 MiB total and 10,000 items. Safe export checks final filename/inode
identity and size before declaring completion.

The 26-test suite includes actual FAT snapshots through GUI and CLI, source-byte equality, malformed
regions, short reads, cancellation, exact carving offsets and output-file replacement regressions.

Recovery components now own their safety-policy values, so default temporary policies cannot leave
dangling references in validators. CLI signal interception applies to image export operations only.
