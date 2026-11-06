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

## Explicit limits and future coverage

- Other filesystem coverage; broader formats beyond JPEG/PDF/ZIP carving.
- ext4 deleted records and journal replay.
- exFAT TexFAT/two-FAT support and damaged/missing boot-region recovery.
- Active document previews and whole-file corruption/quality assessment. Bounded raster thumbnails and text/hex inspection are available.
- More damaged-volume reconstruction, missing boot/MFT metadata, fragmented deleted chains,
  sparse/compressed/encrypted data and larger file-count/file-size limits.
- Hardware-aware rescue scheduling and OS-independent read timeouts. Scan discovery and durable rescue capture continuation are implemented.
- Active system-volume recovery. Linux UDisks2 read-only authorization and Windows UAC launch are implemented; source mount/offline state is still prepared outside the app.
- Dedicated physical USB/SATA/NVMe and interrupted-write qualification, independent code review,
  application licensing and signing. Runtime notices are packaged.

Do not describe this milestone as a complete general recovery product or Disk Drill equivalent.
Existing tests establish bounded synthetic behavior only. No local device/app tests are authorized
by this change; run validation on CI or a dedicated evaluation machine.

## exFAT milestone (see exact-head CI)

Automatic scanning now recognizes single-FAT exFAT 1.0 volumes independently of
partition entries. Recovery validates the boot-region checksum, directory entry
checksums, UTF-16 names, cluster chains and allocation bitmap, rejecting cross-linked files and filesystem metadata aliases. Existing files can
be contiguous or fragmented; retained deleted entries are offered only when all
referenced clusters are currently free. Uninitialized logical tails are zero-filled.
Names and folders are preserved through the existing selected-only export flow.

Limits: 2 GiB/file, 1,048,576 clusters/chain, 16 MiB/metadata stream, 64 MiB
aggregate metadata, 1,024 directories and 32 nested levels. Deleted directories,
TexFAT, missing allocation bitmaps, invalid boot checksums and damaged entry sets
are rejected; signature fallback remains available. Checks do not prove that
deleted contents are intact. Tests run in GitHub CI, never on the user's PC.

## Candidate inspection milestone (see exact-head CI)

The results screen can inspect the highlighted file before any export. It reads
at most 64 KiB from the current scan, displays plain text or hexadecimal, and
creates no output files. Discovery replay runs on the existing cancellable worker
thread; changing source and concurrent recovery are blocked until it finishes.
Inspection rejects names outside the current catalog and changed file sizes. It
is intentionally a partial content view, not a file-format validity assessment.

CI also exercises a standard-formatted exFAT filesystem on a disposable Linux
loop device, recovering a nested binary file and a document with exact byte and
source-preservation checks. The fuzz corpus includes a valid exFAT seed.

## Saved catalogs and Linux ext4 (see exact-head CI)

Drive scan catalogs can be saved and reopened through File menu actions, using an
anchored, exclusive new state file on another drive. Restore is transactional and
validates bounds, duplicate paths, volume overlaps, source geometry and a SHA-256
fingerprint over identity plus up to 64 evenly spaced 64 KiB samples. This detects
changes in those samples; it cannot establish equality of unsampled disk bytes.
Preview/export still replays current-source reconstruction. Paused discovery can be saved and continued from its cursor; a partially scanned filesystem volume is replayed. Existing image-operation sessions remain separate.

Automatic discovery also recognizes surviving primary ext4 superblocks after a
lost partition table. Bounded retained allocated regular files are reconstructed
from directory entries, inode/block allocation bitmaps and extent trees. Sparse
and unwritten ranges become zero bytes. Symlinks/special files, non-UTF8 names,
legacy indirect maps, inline/encrypted/verity/bigalloc layouts, deleted records
and journal replay are unsupported. CRC32C checksums are verified when the filesystem enables them, including the
superblock, group descriptors, allocation bitmaps, inodes, extent nodes and linear
directory leaf blocks and htree index nodes. Bounded indexed directories are traversed with checked references. Cross-linked and known-metadata-alias
ranges are excluded. Limits: 4,096 groups, 2 GiB/file, 16 MiB/directory, 1,024
directories/32 levels, 10,000 candidates, 100,000 extents and 64 MiB metadata reads.

CI adds a standard-formatted ext4 device fixture with nested binary/document byte
comparisons, parser fuzz seed, corrupt extents/allocation cases and saved-catalog
round-trip, malformed state, changed-source and GUI save/open/recover cases.

## Explicit rescue capture (see exact-head CI)

File menu/CLI rescue capture copies read-only sources to an exclusively created
image on a validated different drive. Batch read failures fall back to single
sectors with two retries (API bound five); unrecoverable sectors are explicitly
zero-filled and listed as sector ranges in a mandatory `.rescue.json` sidecar.
Identity/access errors remain fatal. Verification reads the held output handle
and compares SHA-256 against the written bytes, avoiding a second media read.

No-gap capture succeeds; capture with gaps is partial (CLI exit 3). The report is
reserved before capture, includes the output digest and verification state, and
uses atomic replacement. Cancellation/failure retains the partial image and durable checkpoint report. Resume verifies source binding, held output identity and committed prefix SHA-256 before discarding any uncommitted tail and continuing. Rescue does not enforce a device read timeout, control head scheduling or provide hardware qualification.
Fault injection covers transient/permanent errors, exact gaps, retries, identity
failure, cancellation, no-overwrite and output verification. CI loop/VHD fixtures
exercise the healthy native path; actual failing hardware remains unqualified.

Filesystem-only scanning is now available by clearing the deep signature-search
checkbox, or with CLI `--quick`. Source discovery skips known volume payloads
instead of rereading every sector within them. Unknown gaps still require search.
The chosen depth is retained by saved catalogs and used again for preview/export.
CI verifies that discovery actually reads fewer sectors and that a filesystem-only
catalog stays filesystem-only after save/open/recover.

Ext4 checksum hardening adds known CRC32C test-vector coverage and independent
checksummed fixtures with corruption injected into each supported metadata type.
The normal mkfs.ext4 fixture exercises its standard metadata checksums. Indexed directories validate root/node/leaf checksums and logical references; unsupported
index variants remain explicit.
