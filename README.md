# RecoverySuite

A desktop data-recovery application for accidentally deleted files, lost partitions and
files made inaccessible after an interrupted write or disconnected drive. Connected
physical disks are the primary source; disk images are an optional source and developer fixture.
The application is under development and is not yet equivalent in recovery coverage to
mature tools such as Disk Drill. Passing synthetic CI does not establish that it can recover
all real-world data-loss cases.

## Recover lost data from a connected drive

1. Open RecoverySuite and select the affected drive from the connected-drive list.
2. Choose **Use selected drive**, then **Search for lost data**. The automatic scan
   searches for surviving FAT12/16/32, exFAT, NTFS and ext4 volumes, even without partition entries,
   reconstructs supported existing/deleted-file candidates, and searches JPEG/PDF/ZIP signatures.
3. Review filenames, paths, sizes and recovery confidence. Filter the list and check the
   files to recover. Use **Inspect highlighted file…** to read a bounded text/hex preview
   before export. Scanning and inspection do not save files or require an output folder.
4. Choose an existing destination folder on a **different physical drive**, then click
   **Recover selected files**. Only selected candidates are copied and read back for verification.
   Existing destination files are preserved. The application never repairs partition tables,
   formats the source, or writes to source sectors.

Linux source volumes must be unmounted; Windows source disks must already be offline.
Appropriate device-read permissions are required. RecoverySuite does not automatically change
mount/offline state or grant privileges. An active system drive is not a supported live source.
The source list comes from actual OS metadata; a listed disk is not a promise that it can be opened.

Deleted-partition recovery currently requires recognizable surviving FAT/exFAT/NTFS boot metadata.
Interrupted writes may leave inconsistent filesystem records; metadata recovery and signature
fallback can find some candidates but cannot recreate unwritten, overwritten or TRIM-discarded bytes.
Supported scope is FAT12/16/32, single-FAT exFAT 1.0, bounded NTFS recovery and JPEG/PDF/ZIP
carving. Inspection shows up to 64 KiB as plain text or hexadecimal, without executing content.
exFAT supports bounded contiguous/fragmented files, subfolders and retained deleted entries
whose clusters remain free; invalid boot/entry checksums and cross-linked allocations are rejected.
Retained allocated ext4 files use directory/inode/extent reconstruction, including sparse and
unwritten ranges. ext4 metadata checksums and journal replay remain unverified, and deleted
ext4 files are not reconstructed from erased records.
TexFAT, encrypted/compressed streams, rich media/document previews, arbitrarily fragmented
deleted files, interrupted scan continuation and failing-media rescue retries remain open work.

Use **File → Save scan results…** to save a catalog on another drive and **Open saved scan…**
to reopen it later. The saved source must still be available at the same path. Reopening checks
identity, geometry and up to 4 MiB of sampled content; it is not a full-disk comparison.
Selected files are reconstructed again for inspection/recovery. Saved catalogs do not continue
a scan that was interrupted before discovery finished.

**Optional image source:** choose **Open an existing disk image instead…** on the source page.
It uses the same automatic scan/review/recover flow. The advanced method controls and the
CLI image commands below remain available for targeted recovery and inspection.

**CLI:** `RecoverySuite_CLI scan /dev/sdX` lists candidates; use
`RecoverySuite_CLI device recover /dev/sdX --output /other-drive/folder --select 'candidate/path'`
to recover chosen paths (repeat `--select` as needed). Windows uses `\\.\PhysicalDriveN`.
For an existing raw image, `scan disk.img --image` and `device recover disk.img --image ...`
use the same flow. No capture-to-image step is required for direct device recovery.

## Build and test on Linux

Install a C++20 compiler, CMake 3.20+, Ninja, Qt6 Core/Gui/Widgets/Test development packages,
and libudev. On Debian/Ubuntu, use `cmake ninja-build g++ qt6-base-dev libudev-dev`.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure --timeout 30
./build/bin/RecoverySuite_GUI
./build/bin/RecoverySuite_CLI help
```

Tests run on synthetic inputs; GUI tests use Qt's offscreen platform. Assertions remain enabled
in Release test targets. CI runs Linux Debug/Release, Windows x64 Release and a bounded Clang sanitizer fuzz job.
See [development packages and release QA](docs/RELEASE_QA.md) for artifacts and validation gates.

## Recover from a disk image

Open a regular, nonempty raw image in the GUI and choose its sector size (512, 1024, 2048 or
4096 bytes). The image length must be sector-aligned. Configure a sector range, select an
existing output folder, and choose **Validate and start recovery**. Saved bytes are read back
and compared by default. Existing files are never replaced; use an empty folder for another run.
The source is opened read-only. Linux rejects replaced, resized or modified sources during reads.

The signature-carving mode scans for contiguous JPEG, PDF and ZIP signatures with explicit end markers.
It saves candidates up to 16 MiB each, with a maximum of 10,000 attempted outputs. These are
signature candidates, not proof of valid file contents. Original names, deleted-file metadata
and fragmented files are not reconstructed. Incomplete/oversized candidates and output failures
are reported; completed files remain available after cancellation or partial failure.

## Recover from the terminal

```sh
./build/bin/RecoverySuite_CLI image info source.img
./build/bin/RecoverySuite_CLI image analyze source.img --start 0 --count 1
./build/bin/RecoverySuite_CLI image carve source.img --output /existing/empty-folder --start 0
./build/bin/RecoverySuite_CLI image metadata fat-volume.img --output /another/existing-folder
./build/bin/RecoverySuite_CLI image files fat-volume.img --output /existing/files-folder
./build/bin/RecoverySuite_CLI image tree fat-volume.img --output /existing/tree-folder
./build/bin/RecoverySuite_CLI image mft ntfs-volume.img --output /existing/mft-folder
./build/bin/RecoverySuite_CLI image ntfs-files ntfs-volume.img --output /existing/small-files-folder
```

Use `--sector-size 4096` for a 4 KiB-sector image, `--count N` to bound the scan, and
`--no-verify` to skip readback comparison. Ctrl+C or SIGTERM cancels cooperatively and keeps
completed outputs. Exit codes: 0 complete, 1 failed/partial, 2 invalid arguments, 130 cancelled.
Native acquisition is available through `device capture` (see below); image recovery runs in the
foreground. The legacy recovery command returns an error instead of reporting unsaved buffers as recovered files.

`image analyze` reports FAT/NTFS boot-sector fields, declared geometry and source-size warnings.
Free/used space, filesystem version and other unexamined properties remain `unknown`. It does not
traverse allocation tables, validate the whole filesystem, or repair the image.

## FAT metadata snapshots

Choose **FAT metadata snapshot** in the GUI, or run `image metadata` from the CLI. Set the start
sector to the FAT volume boot sector and select a range containing its metadata. Outputs contain
actual bytes: the boot sector, each FAT copy, and the fixed root directory on FAT12/16. FAT32 root
cluster chains, NTFS MFT reconstruction, deleted-file recovery and metadata repair are unavailable.
The reader sector size must match the BPB. Each region is limited to 16 MiB; malformed geometry,
short reads and out-of-range regions fail before any output is saved. Existing outputs are preserved.

## FAT12/16/32 root files

Choose **FAT12/16/32 allocated root files (long names / 8.3)** or run `image files`. Select the volume boot sector
and its complete declared sector range. This copies allocated root files, including fragmented
cluster chains, with verified Unicode long names or ASCII 8.3 aliases. Zero-byte files are preserved.
Deleted entries and subdirectories are skipped; subdirectories are not traversed. NTFS files and
guessed deleted-file chains are unsupported. Each file is limited to 16 MiB.

Mirrored FAT copies must agree on cluster values. FAT32 volumes with mirroring disabled use the
selected active FAT. FAT32 root directories are followed through their cluster chain (16 MiB maximum),
and their clusters cannot be reused as file data. Invalid names, cyclic/short/overlong/bad/out-of-range/cross-linked chains
are reported as skipped invalid files. Completed outputs are kept after a later error or cancellation.
Long names require a contiguous, ordered LFN sequence with matching short-name checksums, valid
UTF-16 and padding. Invalid sequences, unsafe names and names exceeding 255 UTF-8 bytes fall back
to the ASCII short alias; the result reports how many names fell back. Original case/OEM short-name
code pages are not reconstructed. Names are never shortened or rewritten to resolve collisions.
Readback verification checks saved bytes; it does not prove that the source filesystem is intact.

## FAT files with subfolders

Choose **FAT12/16/32 files with subfolders** or run `image tree`. This follows allocated directory
and file chains, including fragmented directories and validated Unicode long names. The complete
volume range is required, as in root-file mode. Directory clusters are reserved before any file
payload is exported so directory/file overlaps cannot be recovered as normal files. Dot/parent
links are skipped. Invalid directories/files cause a failed/partial result; completed outputs remain.

Traversal is limited to 32 levels, 1023 attempted subdirectories, 16 MiB per directory and 64 MiB
of directory reads in total, plus the existing 10,000-file and 16 MiB/file limits. Hitting the global
directory/file-count limit aborts discovery before export. Empty directories, original permissions,
timestamps, deleted-chain inference and NTFS are not exported. Directory paths are preserved only
for saved files. Path components must fit 255 UTF-8 bytes and the full relative path 4095 bytes.

On POSIX the writer creates missing folders with mode 0700, reuses existing real directories,
rejects symlinks in every component and creates files exclusively with mode 0600. Output failures
and cancellation may leave empty created folders. Existing output files are never replaced or
renamed to resolve name collisions. Cancellation removes the operation's incomplete file.

## NTFS MFT snapshot

Choose **NTFS boot and initialized MFT snapshot**, or run `image mft`. Select the volume boot
sector and its complete declared sector range with the correct sector size. The reader locates
record zero from the boot sector and follows its unnamed DATA mapping pairs, including backward
and fragmented extents. It checks ranges, overlapping runs, FILE headers and update-sequence
fixups before writing. Outputs **NTFS_BootSector** and **NTFS_MFTInitialized** preserve the raw
on-disk bytes; fixups are applied only to parsing copies. The latter includes only initialized
MFT bytes, including unused/zero-filled slots. Reported counts describe record flags/slots.

The initialized stream is limited to 16 MiB, records to 512–65536 bytes and mapping pairs to 4096
runs. Record zero must fit contiguously within the first run. Attribute-list extensions, sparse,
compressed/encrypted MFT data and MFT-mirror fallback are unsupported. Invalid metadata or a
failed/short read prevents output; later output failures retain completed files. This is a bounded
metadata snapshot, not NTFS user-file reconstruction, metadata repair or whole-volume validation.

## NTFS root files

Choose **NTFS root files (resident / nonresident)** or run `image ntfs-files`. This copies the unnamed
resident or nonresident DATA stream from in-use base FILE records, with the initialized-MFT snapshot limits above.
A root FILE record must exist; each chosen FILE_NAME parent reference must match its record number
and sequence. UTF-16 names are decoded without lossy replacements. One preferred safe root name
is exported per record, favoring Windows names over DOS aliases; other names are counted but are
not recreated as hard links. Empty resident files are preserved. File bytes include restored USA
words, rather than on-disk protection tags. Output names are never rewritten to resolve collisions.

Nonresident DATA follows complete base-record mapping pairs, including fragmented runs and negative
LCN deltas. Each logical file is limited to 16 MiB and 4096 runs; the operation tracks at most 65536
physical extents. Only initialized bytes are read; the logical tail is zero-filled. Allocation slack
is never exported. Run sizes, volume bounds and intra-file overlaps are checked. MFT extents are
reserved, and a later file overlapping an earlier file's claimed extents is rejected; an earlier
completed output remains available. Failed payload reads do not export that file.

Non-root links, directories, extension records and unused user slots are skipped. Unsupported or
invalid root files produce a failed/partial result while preserving completed outputs.
Compressed/encrypted/sparse/reparse files, attribute-list extensions, deleted reconstruction,
alternate-stream export and recursive NTFS traversal are unsupported. Allocation
bitmaps and directory indexes are not cross-checked; in-use record flags and parent references do
not establish whole-filesystem consistency. Original permissions/timestamps are not restored.

## NTFS subfolders

Use GUI **NTFS files with subfolders** or CLI `image ntfs-tree`. This resolves in-use directory
parents with sequence checks and emits safe relative paths through the hierarchical exporter.
Limits: 32 directory levels, 1023 directories and 4095 path bytes. Cyclic, missing, stale and unsafe
ancestors are rejected. Empty directories are not exported. This mode does not yet validate directory
indexes or allocation bitmaps.

## NTFS checked recovery and larger files

GUI **NTFS files with allocation and directory checks**, or CLI `image ntfs-checked`, validates
MFT allocation flags, volume cluster bits and parent directory index membership before exporting
in-use files. Resident INDEX_ROOT and allocated INDX children are checked for bounds, VCNs, bitmap
membership, update-sequence fixups, cycles and unreachable allocated blocks. This verifies selected
metadata relationships; it does not certify file contents or repair the volume.

User-file ATTRIBUTE_LIST records may be resident or nonresident (up to 1 MiB). References must match
record sequence, base reference, attribute type/name/instance and contiguous VCN ranges. Up to 64
extension records and 4096 physical runs per file are supported; nested lists and MFT bootstrap
attribute-list extensions remain unsupported. Sparse, compressed and encrypted streams remain unsupported.

The checked mode streams files up to 2 GiB through 64 KiB buffers, including source readback during
verification. Initialized MFT capacity is 128 MiB, allocation/index metadata streams are limited to
16 MiB, and output remains limited to 10000 files. The existing snapshot/root/tree compatibility
modes retain their earlier limits and do not imply allocation/index validation. Failed/cancelled
streamed output is removed without replacing any existing file.

## Deleted file candidates

Use GUI **NTFS deleted file candidates** / CLI `image ntfs-deleted` for unused base records with
retained names and DATA. MFT bitmap flags must agree; all nonresident file clusters must currently
be free. Live data and known metadata extents are reserved. Retained attribute-list extensions and
deleted parent directories can be used only with matching sequence/base references. Stale parents,
reallocated clusters and unsupported data features are rejected. Limits match checked recovery.

Use GUI **FAT deleted file candidates (contiguous assumption)** / CLI `image fat-deleted` for deleted
short entries in reachable live directories. This assumes the file occupied contiguous clusters from
its retained first cluster, and requires every candidate cluster to be free and unclaimed. Live chains,
conservative live contiguous extents and directory clusters are reserved first. FAT files remain limited
to 16 MiB; cluster analysis is capped at 16 million steps. Generated names start with `deleted-` and
replace the lost first short-name character with `_`. Deleted LFN names, OEM aliases, deleted directory
traversal and arbitrary fragmented deleted chains are not reconstructed.

These outputs are **candidates**: a currently free cluster may contain overwritten or unrelated bytes.
NTFS metadata checks and output readback do not establish original content. FAT confidence is explicitly
low because its cleared chain must be inferred. Names are never rewritten to replace an existing output.
Partial failures/cancellation preserve completed files and remove the current incomplete output.
NTFS allocation analysis is capped at 64 million cluster-bit checks and 100000 index entries in total.

## Save and resume a recovery session

Add `--session /existing/folder/scan.json` to an image export command, then use
`RecoverySuite_CLI session resume /existing/folder/scan.json`. In the GUI, enter an optional new
session filename in Recovery settings; use **File → Resume session** to review and restart a saved
operation. The session stores versioned settings, the full source SHA-256, and pending/completed
output names, sizes and SHA-256 values. One process may use a session at a time.

Resume fingerprints the entire source again and replays discovery from the beginning. Earlier outputs
are read back and compared with the newly discovered source bytes; unchanged outputs are reused and
remaining items are retried. This avoids losing signature boundaries or filesystem traversal state.
It does not resume midway through a file or skip initial metadata discovery. Full fingerprinting adds
one sequential source pass. Changed sources, changed outputs, malformed schemas and configuration
mismatches fail explicitly. Files are never overwritten. A hard kill can leave an incomplete output;
resume reports that conflict rather than deleting an unverified file. Normal cancellation cleans it up.
Session files use atomic replacement; do not select the source image as the session filename.

## Current state

- GUI and CLI build on Linux and Windows x64. All 30 registered CTest executables passed
  Linux Debug/Release and Windows Release CI at `8de6f93` (run `41905828702`).
  Newer revisions require their own CI results; no application tests run on the user's workstation.
- The GUI connects real image selection, range/output validation, worker scanning, safe file export
  and results. Progress and cancellation reach the streaming scan and output loops.
- POSIX output export rejects traversal, symlink components, unsafe names and existing files. It
  anchors the selected directory with a file descriptor; incomplete writes are removed on failure.
- The image workflow supports signature carving, FAT metadata snapshots and allocated FAT12/16/32 root
  files, with optional bounded recursive traversal. NTFS root files support resident and bounded nonresident DATA. NTFS tree and checked modes support subfolders; separate deleted modes export explicitly labelled candidates.
- Legacy NTFS factory inspection reads actual 64-bit boot fields. Unexamined allocation, mount and
  repair metrics throw `std::logic_error`; version/label remain unknown, and the legacy analyzer
  reports full consistency analysis unavailable. Legacy FAT inspection also rejects malformed boot
  geometry and reports unexamined health/allocation/partition properties as unavailable, rather than
  manufacturing healthy or zero values. Use the image workflows for supported MFT/file work.
- Windows Unicode image access, directory-anchored export, sessions and read-only acquisition
  passed synthetic CI, including a disposable VHD. Physical USB/SATA/NVMe and failing-media
  behavior remain unqualified on both platforms.
- Integration tests use synthetic images; CI also fuzzes parsers with AddressSanitizer/UBSan.
  Earlier local validation is historical. Continue validation only on CI or a dedicated test machine.

See [the active roadmap](docs/MASTER_TODO.md), [session state](docs/SESSION_STATE.md),
and [the handoff](docs/HANDOFF.md) before continuing development.

### Native device acquisition (development)

Use `RecoverySuite_CLI device capture <device> --output <new-image>` with an existing output
parent directory on a different physical disk. Linux device names are e.g. `/dev/sdb`; Windows
uses `\\.\PhysicalDrive2`. Device access requires OS permissions. Linux refuses mounted sources;
Windows requires an already offline source disk. The application never takes disks offline or
writes source sectors. Read errors stop acquisition; this is not a damaged-drive retry tool.
Outputs are exclusive, streamed, read back and SHA-256 reported; cancellation removes incomplete
output. Open the resulting image with GUI/CLI recovery and its optional recovery session.

Enumeration reports actual OS metadata only. SMART/NVMe health, NAND estimates and vendor commands
are unavailable. Windows native export and acquisition are development features. Check the exact-head CI results
and complete real-device QA; synthetic image/device tests do not establish hardware validation.

### Build on Windows x64

Use Visual Studio 2022 C++ tools, CMake and Qt 6.8+ MSVC x64 (Core/Gui/Widgets/Test). Make Qt's
`bin` directory available on PATH / CMAKE_PREFIX_PATH before configuring:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure --timeout 60
cd build
cpack -C Release -G ZIP
```

The CLI accepts Unicode arguments through the native Windows wide-character entry point.
Windows output is limited to local drive paths; unsafe/reserved names and reparse points are
rejected. The portable package bundles Qt; see RELEASE_QA.md for runtime prerequisites.

## Linux and Windows prototype delivery

Both platforms are release targets. The `Build and test` workflow builds Linux Debug/Release
and Windows x64 Release, validates disposable loop/VHD devices and package contents, and fuzzes
parsers. Test reports are preserved even when a test fails. Once every required job passes,
the `RecoverySuite-prototype` artifact combines Linux TGZ/Ubuntu 24.04 DEB and Windows ZIP,
installation notes, source revision and SHA-256 manifest. A failing platform prevents this
combined prototype from being produced. Platform development artifacts can remain available
individually; their existence does not establish a successful combined prototype.

Current instruction: perform application/device/package tests on CI runners or a dedicated
evaluation machine, not on the user's workstation. No CI success is assumed without checking
the exact revision. Prototypes are development artifacts, not signed/stable releases.
