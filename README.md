# RecoverySuite

A C++20/Qt6 desktop toolkit for storage analysis and data recovery, with GUI and CLI interfaces.

This is an unfinished development project. Architecture documents describe the intended scope;
phase completion notes are historical and do not establish production readiness.

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
in Release test targets. CI runs both Debug and Release builds on Linux.

## Recover from a disk image

Open a regular, nonempty raw image in the GUI and choose its sector size (512, 1024, 2048 or
4096 bytes). The image length must be sector-aligned. Configure a sector range, select an
existing output folder, and choose **Validate and start recovery**. Saved bytes are read back
and compared by default. Existing files are never replaced; use an empty folder for another run.
The source is opened read-only. Linux rejects replaced, resized or modified sources during reads.

The image workflow scans for contiguous JPEG, PDF and ZIP signatures with explicit end markers.
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
Physical-device export and background operation tracking are unavailable; the legacy recovery
command returns an error instead of reporting unsaved buffers as recovered files.

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

## NTFS resident root files

Choose **NTFS resident root files (small files)** or run `image ntfs-files`. This copies the unnamed
resident DATA stream from in-use base FILE records, with the initialized-MFT snapshot limits above.
A root FILE record must exist; each chosen FILE_NAME parent reference must match its record number
and sequence. UTF-16 names are decoded without lossy replacements. One preferred safe root name
is exported per record, favoring Windows names over DOS aliases; other names are counted but are
not recreated as hard links. Empty resident files are preserved. File bytes include restored USA
words, rather than on-disk protection tags. Output names are never rewritten to resolve collisions.

Non-root links, directories, extension records and unused user slots are skipped. Root files with
nonresident DATA or unsupported features produce a failed/partial result while preserving completed
outputs. Compressed/encrypted/sparse/reparse files, attribute-list extensions, deleted/nonresident
reconstruction, alternate-stream export and recursive NTFS traversal are unsupported. Allocation
bitmaps and directory indexes are not cross-checked; in-use record flags and parent references do
not establish whole-filesystem consistency. Original permissions/timestamps are not restored.

## Current state

- GUI and CLI compile on Linux; all 28 registered CTest executables pass locally in Debug and Release.
- The GUI connects real image selection, range/output validation, worker scanning, safe file export
  and results. Progress and cancellation reach the streaming scan and output loops.
- POSIX output export rejects traversal, symlink components, unsafe names and existing files. It
  anchors the selected directory with a file descriptor; incomplete writes are removed on failure.
- The image workflow supports signature carving, FAT metadata snapshots and allocated FAT12/16/32 root
  files, with optional bounded recursive traversal. Deleted chains, NTFS nonresident files and NTFS directory traversal remain unavailable.
- Legacy NTFS factory inspection reads actual 64-bit boot fields. Unexamined allocation, mount and
  repair metrics throw `std::logic_error`; version/label remain unknown, and the legacy analyzer
  reports full consistency analysis unavailable. Use the image workflows for supported MFT/file work.
- Windows safe export is unavailable; Windows builds and recovery against real devices remain unvalidated.
- Integration tests use synthetic images; the image workflow also passes AddressSanitizer/UBSan locally.

See [the active roadmap](docs/MASTER_TODO.md), [session state](docs/SESSION_STATE.md),
and [the handoff](docs/HANDOFF.md) before continuing development.
