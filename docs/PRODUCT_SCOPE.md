# RecoverySuite product direction

Updated: 2026-11-06. This supersedes the earlier image-first milestone framing.

## User objective

A general desktop disk data-recovery application, in the category of Disk Drill:
select the affected connected disk, find files after accidental deletion/lost
partitions or interrupted writes, review what can be recovered, and copy selected
files to another drive. Images are optional inputs and test fixtures, not the product.
No completion percentage is meaningful without coverage of those user scenarios.

## Connected-drive prototype

- Actual connected-drive inventory in the primary source screen.
- Direct native read-only access, preserving unmounted/offline and different-disk output guards.
- Automatic whole-disk FAT/exFAT/NTFS boot-volume discovery independent of partition entries.
- Existing/deleted metadata reconstruction plus whole-source signature fallback.
- Scan-only candidate catalog: names/paths, sizes and explicit confidence; no export during scan.
- Search/filter/check selection and separately requested, verified recovery to another drive.
- Images use the same workflow; manual methods are under Advanced.
- Dedicated CI cases cover an erased partition table, damaged boot metadata, selective
  recovery, no-overwrite, source preservation, cancellation, stale catalog rejection and GUI flow.
- Disposable Linux loop/Windows VHD jobs exercise direct device scan and selected-file recovery.

## Remaining product work

- Other filesystem coverage (including ext4); broader formats beyond JPEG/PDF/ZIP carving.
- exFAT TexFAT/two-FAT support and damaged/missing boot-region recovery.
- Rich media/document previews and corruption/quality assessment; bounded text/hex inspection is available, but does not prove a whole file is intact.
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

## exFAT milestone (validation pending)

Automatic scanning now recognizes single-FAT exFAT 1.0 volumes independently of
partition entries. Recovery validates the boot-region checksum, directory entry
checksums, UTF-16 names, cluster chains and allocation bitmap. Existing files can
be contiguous or fragmented; retained deleted entries are offered only when all
referenced clusters are currently free. Uninitialized logical tails are zero-filled.
Names and folders are preserved through the existing selected-only export flow.

Limits: 2 GiB/file, 1,048,576 clusters/chain, 16 MiB/metadata stream, 64 MiB
aggregate metadata, 1,024 directories and 32 nested levels. Deleted directories,
TexFAT, missing allocation bitmaps, invalid boot checksums and damaged entry sets
are rejected; signature fallback remains available. Checks do not prove that
deleted contents are intact. Tests run in GitHub CI, never on the user's PC.

## Candidate inspection milestone (validation pending)

The results screen can inspect the highlighted file before any export. It reads
at most 64 KiB from the current scan, displays plain text or hexadecimal, and
creates no output files. Discovery replay runs on the existing cancellable worker
thread; changing source and concurrent recovery are blocked until it finishes.
Inspection rejects names outside the current catalog and changed file sizes. It
is intentionally a partial content view, not a file-format validity assessment.
