# RecoverySuite completion criteria

This is the completion checklist for the requested general Windows/Linux disk
recovery application. It replaces progress-by-percentage and historical image-only
phase summaries. A row is complete only after its implementation and CI evidence
exist. Unsupported filesystem variants remain explicit product limits; this is
not a promise of Disk Drill feature parity.

## Software acceptance

- [x] Connected physical-drive inventory and guarded read-only source selection.
- [x] Lost-partition discovery, FAT/exFAT/NTFS and retained ext4 file recovery.
- [x] Review, filter, inspect and selectively export files to another drive.
- [x] Explicit overwritten/unsupported/partial result reporting and no-overwrite.
- [x] Save/reopen completed drive catalogs with source checks.
- [x] Continue interrupted rescue capture from flushed, verified checkpoints.
- [x] Save/continue interrupted scan discovery without losing found candidates.
- [x] Usable user-authorized privileged source access on Windows and Linux.
- [x] Bounded visual/file previews beyond the text/hex inspector.
- [x] Unified help, clean packaging, runtime notices and updated completion evidence.
- [x] Required exact-head Windows/Linux CI, corruption/fault cases and package integrity gates.

## External release gates

Physical USB/SATA/NVMe and interrupted-write/failing-media qualification must use
a separate evaluation machine. No testing on the user's PC is authorized. An
independent review, application licensing decision and signing credentials are
release decisions, not results that code or synthetic CI can fabricate.

Do not mark the full release complete while required external gates remain open.

## Validation record

Rescue restart, paused-scan continuation and raster previews passed all six jobs at `6b73636ccb210cf992d52568c7198f6aa5158f43`, run [42175036619](https://github.com/morbok/RecoverySuite/actions/runs/42175036619). The Linux delivery job at `7a4d0523d556659c2f3463355fd259735242df8f` also passed real UDisks2 descriptor transport, all CTest suites, native loop/ext4/exFAT checks and installed/extracted package checks. Later code and the combined deliverable must have their own exact-head run. The final artifact manifest records the source revision and workflow; an artifact only exists after all required jobs succeed.

Authorization, embedded help, complete runtime-notice collection/hashes and installed/extracted packages passed all six jobs at `7a4d0523d556659c2f3463355fd259735242df8f`, run [42176003051](https://github.com/morbok/RecoverySuite/actions/runs/42176003051). The latest deliverable is accepted only after its own six jobs succeed; consult its manifest rather than treating an older green run as validation of newer code. All software acceptance features above are implemented; the external release gates remain open.
