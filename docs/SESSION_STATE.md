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

24 CTest executables pass locally in Debug and Release with assertions active. The image integration
suite also passes AddressSanitizer/UBSan. Tests cover source mutation, invalid ranges/destinations,
symlinks/hardlinks/traversal, no-overwrite/partial output, cancellation, incomplete/oversized candidates,
GUI clicks and closing during recovery. Source hashes and saved bytes are checked on synthetic images.
Remote CI must be checked for the current PR head before merge.

## Limits and next work

Only the new image GUI workflow exports files. Legacy file/metadata recovery, CLI export, raw-device
discovery, FAT/NTFS completeness, fragmentation, resumable sessions and Windows export remain work.
The signature carver does not validate full file formats. Limits: 16 MiB/candidate, 10,000 attempted
outputs, 64 KiB scan chunks. Windows and real hardware have not been validated.

Graphify 0.9.97 was installed as a local developer tool and Codex skill. The ignored graphify-out/
contains the AST graph, report, HTML and query memory. Use scoped queries first and update after edits;
inferred call edges require source verification. No new runtime dependency was added to the product.
