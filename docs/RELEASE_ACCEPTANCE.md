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
- [ ] Continue interrupted rescue capture from flushed, verified checkpoints.
- [ ] Save/continue interrupted scan discovery without losing found candidates.
- [ ] Usable user-authorized privileged source access on Windows and Linux.
- [ ] Bounded visual/file previews beyond the text/hex inspector.
- [ ] Unified help, clean packaging, runtime notices and updated completion evidence.
- [ ] Final exact-head Windows/Linux CI, corruption/fault cases, package checks.

## External release gates

Physical USB/SATA/NVMe and interrupted-write/failing-media qualification must use
a separate evaluation machine. No testing on the user's PC is authorized. An
independent review, application licensing decision and signing credentials are
release decisions, not results that code or synthetic CI can fabricate.

Do not mark the full release complete while required external gates remain open.
