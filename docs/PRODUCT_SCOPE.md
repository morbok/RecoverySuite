# RecoverySuite product direction

Updated: 2026-11-05. This supersedes the earlier image-first milestone framing.

## User objective

A general desktop disk data-recovery application, in the category of Disk Drill:
select the affected connected disk, find files after accidental deletion/lost
partitions or interrupted writes, review what can be recovered, and copy selected
files to another drive. Images are optional inputs and test fixtures, not the product.
No completion percentage is meaningful without coverage of those user scenarios.

## Implemented transformation; awaiting exact-head CI

- Actual connected-drive inventory in the primary source screen.
- Direct native read-only access, preserving unmounted/offline and different-disk output guards.
- Automatic whole-disk FAT/NTFS boot-volume discovery independent of partition entries.
- Existing/deleted metadata reconstruction plus whole-source signature fallback.
- Scan-only candidate catalog: names/paths, sizes and explicit confidence; no export during scan.
- Search/filter/check selection and separately requested, verified recovery to another drive.
- Images use the same workflow; manual methods are under Advanced.
- Dedicated CI cases cover an erased partition table, damaged boot metadata, selective
  recovery, no-overwrite, source preservation, cancellation, stale catalog rejection and GUI flow.
- Disposable Linux loop/Windows VHD jobs exercise direct device scan and selected-file recovery.

## Remaining product work

- exFAT and other filesystem coverage; broader formats beyond JPEG/PDF/ZIP carving.
- Rich file previews and corruption/quality assessment; browsing a candidate is not proof it is intact.
- More damaged-volume reconstruction, missing boot/MFT metadata, fragmented deleted chains,
  sparse/compressed/encrypted data and larger file-count/file-size limits.
- Physical scan persistence, fast rediscovery and robust failing-media rescue acquisition/retries.
- Usable privileged device-access integration; currently native permissions and source state
  must already permit read-only access. Active system-volume recovery is not implemented.
- Dedicated physical USB/SATA/NVMe and interrupted-write qualification, independent code review,
  distribution licensing/notices and signing.

Do not describe this milestone as a complete general recovery product or Disk Drill equivalent.
Existing tests establish bounded synthetic behavior only. No local device/app tests are authorized
by this change; run validation on CI or a dedicated evaluation machine.
