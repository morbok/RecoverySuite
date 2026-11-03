# RecoverySuite session state

Updated: 2026-10-31

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

30 CTest executables pass locally in Debug and Release with assertions active. The image, FAT metadata,
FAT file, NTFS, persistent-session and native-I/O integration suites also pass AddressSanitizer/UBSan. Tests cover source mutation, invalid ranges/destinations,
symlinks/hardlinks/traversal, no-overwrite/partial output, cancellation, incomplete/oversized candidates,
GUI clicks and closing during recovery. Source hashes and saved bytes are checked on synthetic images.
GitHub Linux Debug and Release checks passed for code revision `7606815ff46a4aef9efe2423abbf80982a1e7a2b`
(run `41153617015`), verified in the PR checks UI on 2026-10-31. Any newer head needs its own CI check
before merge; code review remains outstanding. Earlier checkpoint CI notes below are historical.

## Limits and next work

GUI and CLI image workflows export signature candidates, FAT metadata, allocated FAT directory trees,
initialized NTFS MFT snapshots and resident/nonresident NTFS root files. Fragmented FAT chains and MFT streams
are supported within documented bounds. Checked NTFS mode now adds directory paths, bounded user-file attribute-list extensions, allocation/index
cross-checks and streamed files up to 2 GiB; separate FAT/NTFS modes export deleted candidates with
explicit uncertainty. MFT bootstrap/nested lists and broader damaged/deleted-chain inference remain work. Persistent
image sessions, native read-only CLI acquisition and anchored Windows export are implemented;
see current platform CI results below and RELEASE_QA.md for remaining hardware/release gates.
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

## Boot-sector analysis checkpoint

Added CLI image analyze for read-only FAT/NTFS boot-field inspection, including nonzero volume starts.
Replaced unaligned integer casts and unchecked initial reads with bounded byte extraction. Removed
false free/used-space, version and label defaults; report explicit unknowns and declared-size warnings.
Fixed GUI service conversion of the backend's actual field names, preserved raw reported properties,
and rejected requests naming a different opened image. Tests cover source binding, exact serial/label
fields, NTFS 64-bit size overflow, unknown metrics, short reads and CLI subprocess output.

## 2026-10-31 — Allocated FAT12/16 root files

Added a bounded FAT root-file reader shared by GUI file_recovery, CLI image files and the legacy
FileRecovery buffer API. Follows actual allocated chains (including fragmentation), preserves ASCII
8.3 aliases and zero-byte files, compares mirrored FATs, accepts FAT16 dirty/error flags, and rejects
invalid geometry, unsafe names, loops, invalid cluster links, short/overlong and cross-linked root
file chains. Cancellation reaches reads and writes; saved files survive later failures. Limits remain
16 MiB/file, 10,000 items; legacy buffered files are capped at 64 MiB. Deleted entries, LFN records
and subdirectories are skipped. FAT32/NTFS file reconstruction remains unsupported.

27 registered CTest executables now include a real fragmented-chain fixture on FAT12 and 4 KiB FAT16,
corruption cases, GUI/CLI output, no-overwrite, source preservation and cancellation.


## 2026-10-31 — FAT32 allocated root-file checkpoint

Extended the shared root-file reader to FAT32. It follows bounded fragmented root-directory
chains, reserves directory clusters against file cross-links, combines directory high/low cluster
words, masks reserved FAT nibbles, accepts dirty/error status flags, and honors active-FAT selection.
Mirrored copies must agree on meaningful cluster values. Unknown versions, malformed geometry,
root-chain corruption and invalid active indexes fail before output. ASCII 8.3 aliases remain the
filename format; no recursive/deleted/NTFS recovery was added. FAT32 metadata snapshot behavior
is unchanged (boot/FAT regions only).

Validation: all 27 CTest executables passed in Debug and Release. Image, metadata snapshot and
FAT-file suites passed ASan/UBSan with leak detection disabled for Qt. FAT32 fixtures cover a
fragmented root, clusters above 65535, active second FAT, ignored high FAT bits, dirty flags,
cycles, cross-links, copy mismatches, invalid versions/flags and cancellation. CLI export is exercised.
Graphify AST update: 3695 nodes / 6468 edges; existing 20 source_file warnings concern config nodes.
Remote CI remains unverified because browser access is unavailable and the connector needs an
account connection. Do not merge until current CI and code review have been checked.


## 2026-10-31 — Unicode FAT long-name checkpoint

Added a bounded, dependency-free LFN decoder for FAT12/16/32 root files. It validates contiguous
reverse ordinal sequences, entry fields, matching short-name checksums, terminators/padding and
UTF-16 (including surrogate pairs). Invalid sequences, unsafe names and names exceeding 255 UTF-8
bytes fall back to ASCII 8.3 aliases. Results expose the fallback count. State is reset across deleted
entries, directories and volume labels so names cannot attach across unrelated records. Exclusive
safe output creation and the existing chain/range limits remain in force. OEM short-name code-page
reconstruction and recursive directories are still pending.

Validation: 27/27 Debug and Release CTest executables; image, FAT metadata and FAT-file integration
suites passed ASan/UBSan (Qt leak detection disabled). Fixtures cover Turkish/emoji names, surrogate
pairs split across records, LFNs split across FAT32 root clusters, 13/255-unit boundaries, unsafe
names, invalid Unicode, checksum/order/type/cluster/padding errors, interruptions and CLI export.
Graphify AST update: 3716 nodes / 6512 edges; 20 existing config-node source_file warnings remain.
Remote CI is unverified; current code review/CI is required before merge.


## 2026-10-31 — Debug: directory/file cross-links

A Debug baseline passed all 27 CTest targets. Two new corrupt-image regression cases then failed:
a root file sharing the first or a later cluster of a child-directory chain was exported as successful.
Root-file recovery now reserves known child-directory chains before any file export, regardless of
entry order, without reading or exporting those subdirectories. Cyclic chains are bounded by the
visited-cluster set and cancellation is checked throughout. Both new regressions pass; all 27 Debug
and Release CTest targets and the image/metadata/FAT-file ASan+UBSan suites pass.


## 2026-10-31 — Recursive FAT tree checkpoint

Added optional GUI FAT files-with-subfolders and CLI image tree, with the same source/range/output
checks and signal cancellation as other image exports. The shared FAT reader discovers allocated
FAT12/16/32 directories, including fragmented chains and cross-cluster Unicode LFN records, before
reading file payloads. Directory claims are reserved even when their names or sizes are invalid;
claimed metadata cannot be exported as file data. Dot/parent links are ignored. A bounded bitmap
tracks cluster ownership. Root-only mode and legacy buffered APIs remain root-only by default.

POSIX tree export validates every path component, anchors traversal with directory handles,
rejects symlinks/traversal and existing files, creates private directories (0700)/files (0600), and
cleans incomplete files on failure or cancellation. Existing real directories can be reused.
Empty directories and original timestamps/permissions are not reconstructed; cancellation/output
failures may leave empty created folders. Limits: 32 directory levels, 1023 attempted subdirectories,
16 MiB per directory, 64 MiB directory reads, 4095 path bytes, 255 component bytes, and existing
16 MiB/file plus 10,000-file caps. Global item-limit failures abort discovery before export.

Validation: all 27 CTest targets pass in Debug and Release; image, metadata snapshot and FAT-file
suites pass ASan/UBSan (Qt leak detection disabled). Recursive tests cover FAT12, FAT16 4K sectors,
FAT32, nonzero volume offsets, actual GUI and CLI actions, Unicode names, fragmentation, nested
and empty files, repeats/no-overwrite, cancellation, directory/file/parent cycles, symlinks,
invalid directory-name claims, directory byte/count and depth limits. Selected recursive GUI and
corruption cases also exited normally under GDB (12 QtTest passes including setup/cleanup).
Graphify updated: 3732 nodes / 6574 edges, with 20 existing config-node source_file warnings.
Remote CI remains unverified; do not merge without checking current CI and code review.

Next release work: OEM short-name encoding, deleted-file reconstruction, NTFS MFT/files, Windows
safe export, persisted sessions/resume, real-device QA and packaging. Do not label the product done.


## 2026-10-31 — NTFS initialized-MFT snapshot checkpoint

Added an independent byte-wise NTFS reader (no packed legacy boot/attribute casts) and GUI/CLI
image mft. Validates 64-bit declared geometry, boot record size, bootstrap FILE header/USA fixups,
resident attribute bounds and unnamed nonresident DATA mapping pairs. Supports positive/negative
LCN deltas and fragmented streams; rejects overlaps, invalid lengths/ranges, sparse/compressed/
encrypted data, attribute-list extensions and noncontiguous bootstrap records. Reads only initialized
MFT bytes, capped at 16 MiB; each nonzero FILE slot has header/fixup validation. Raw output bytes
are preserved, including update-sequence tags, unused slots and zero-filled slots. Counts describe
header flags/slots, not confirmed user-file counts. Metadata failures precede any output writes.

The existing safe exporter handles boot/MFT outputs, no-overwrite, verification and cancellation.
New test_ntfs_mft covers actual GUI and CLI actions, 512/4096-byte sectors, backward/fragmented
runs, attributes crossing a 512-byte fixup stride, malicious geometry/attributes/runlists, torn later
records, source preservation, cancellation and partial output conflicts. A sparse reader validates
LCNs/volume sizes above 32 bits at a nonzero partition offset, plus short/oversized/failed reads.

Validation: 28/28 Debug and Release CTest targets pass. Image, FAT metadata, FAT files and NTFS MFT
integration suites pass ASan/UBSan (Qt leak detection disabled). Selected NTFS workflows exit normally
under GDB, 7 QtTest passes including setup/cleanup. Graphify: 3798 nodes / 6721 edges; 20 existing
config-node source_file warnings. Remote CI remains unverified; no merge. Next: actual NTFS user-file
reconstruction. Legacy NTFS factory structs/analyzer remain unvalidated and are not used by this path.


## 2026-10-31 — NTFS resident root-file checkpoint

Added GUI NTFS resident root files and CLI image ntfs-files. Read initialized MFT with the shared
validated reader, then copy unnamed resident DATA from in-use user base records. Check the root
record and parent sequence references, STANDARD_INFORMATION/FILE_NAME/DATA bounds, required
metadata, duplicate data attributes and unsafe Unicode names. Restore USA words before copying
payloads, including data that crosses a 512-byte stride. Empty resident files are preserved.
Choose one preferred safe root name per record (Windows before DOS); count other names not exported.
Skip unused, directory, extension and non-root records. Unsupported/nonresident root files make the
result failed/partial; completed output is retained. No allocation-bitmap/index cross-check,
nonresident/deleted/recursive recovery, ADS export or original permissions/timestamps.

The attribute walker is shared with MFT bootstrap parsing. FAT and NTFS share a bounded UTF-16 to
UTF-8 filename decoder; FAT regression tests remain green. Cancellation, source identity and safe
flat POSIX output behavior are reused. Added real GUI/CLI export tests at nonzero volume offsets
with 512/4096 sectors, Unicode and empty files, exact restored payload bytes, unused/non-root/extension
records, stale root references, invalid names and attribute bounds, duplicate/missing attributes,
unsupported file features, root validation, DOS-name preference, repeats and cancellation.

Validation: 28/28 CTest targets in Debug and Release; image, FAT metadata, FAT files and NTFS suites
pass ASan/UBSan (Qt leak detection disabled). Selected resident tests exit normally under GDB:
22 QtTest passes including setup/cleanup. Graphify: 3839 nodes / 6834 edges; 20 existing config-node
source_file warnings. Remote CI remains unverified; no merge. Remaining NTFS scope includes
nonresident file streams, directory traversal, allocation/index consistency and attribute lists.


## 2026-10-31 — Legacy NTFS boot/inspection audit checkpoint

Corrected NTFS boot/header models to their real wire offsets: 512-byte boot sector, 64-bit totals
and MFT locations, record-size encodings at 64/68, 8-byte serial at 72 and signature at 510.
Added compile-time layout checks; corrected resident attribute flag positions, NTFS 3.x FILE header
size and index-entry key length. Runtime legacy metadata/probe/volume reads now use a bounded
byte-wise helper rather than packed casts; check exact OEM ID, sector/cluster/record geometry,
64-bit multiplication, declared source bounds and short reads. Report full serial, declared size
and cluster counts. Volume offsets come from the reader; end-offset overflow returns unavailable.

Removed fixed version/label, fabricated allocation/mount/partition flags, fake MFT validation,
corruption counts and consistency/repair claims. Version/label stay Unknown; unavailable numeric/
boolean metrics throw std::logic_error, as documented. The legacy analyzer returns false for
unavailable full consistency analysis and explains which fields remain unknown. FilesystemReader
access is read-only. The supported GUI/CLI MFT and resident-file workflows remain independent.
Fixed NTFSAnalyzer.h resolving the recovery analyzer header instead of its filesystem base when
both API families are included; use an explicit relative base-header include.

Regression coverage checks layouts, >32-bit sizes/LCNs, complete serials, cluster geometry,
short reads, malformed OEM IDs, unsupported metrics and offset overflow. All 28 CTest targets
pass in Debug and Release; all four image/FAT/NTFS integration suites pass ASan/UBSan with Qt leak
detection disabled. Legacy regression exits normally under GDB (3 passes including setup/cleanup).
Graphify: 3861 nodes / 6866 edges; 20 existing config-node source_file warnings. Remote CI remains
unverified; no merge. Still open: nonresident/recursive NTFS files, allocation/index consistency,
deleted recovery, Windows safe export, persisted sessions, hardware QA and packaging.

## 2026-10-31 — Verification checkpoint

Rebuilt Debug, Release and ASan/UBSan configurations at code revision `7606815` and reran validation:
28/28 Debug tests (3.96 s), 28/28 Release tests (1.62 s), and 4/4 sanitizer integration suites
(14.12 s; ASAN_OPTIONS=detect_leaks=0 for Qt). Local HEAD matched the remote branch before this
documentation update. No source changes or newly failing tests were found.

Verified both GitHub Linux jobs succeeded for that exact code revision:
https://github.com/morbok/RecoverySuite/actions/runs/41153617015
This checkpoint updates the current validation/scope summary above; it does not imply release
completion, Windows/hardware validation or completed code review.

## 2026-10-31 — NTFS nonresident root-file checkpoint

Extended GUI NTFS root files and CLI image ntfs-files to unnamed nonresident DATA in in-use root
base records. Reuse mapping-pairs decoding with MFT snapshots; validate complete VCN coverage,
physical bounds, size relationships and run overlaps. Support fragmented/backward runs, trim allocation
slack and zero-fill the uninitialized logical tail. Reserve MFT and earlier-file runs; later conflicts
fail while earlier completed outputs remain. Payload read failures do not export the affected file.
Keep the resident-only public helper as a compatibility wrapper. Limits: 16 MiB/file, 4096 runs/file,
65536 tracked extents, plus existing MFT and output limits. Source identity, safe output and cancellation
are shared with existing workflows. No allocation/index consistency, directory traversal, attribute-list
extensions, deleted reconstruction, sparse/compressed/encrypted data or ADS export.

Validation: 28/28 Debug CTest executables (3.87 s), 28/28 Release (1.67 s), and 4/4 ASan/UBSan
integration suites (14.45 s; Qt leak detection disabled). New GDB-selected tests exit normally,
22 QtTest passes including setup/cleanup. Cover GUI/CLI bytes at nonzero offsets and 512/4096 sectors,
fragmented negative-delta runs, exact logical lengths, zero/partial/full initialization, no-overwrite,
cancellation during payload reads, malformed mappings, self/MFT/prior-file overlaps, and short,
oversized or failed payload reads. New revision still needs its own remote CI and code review.

## NTFS directory-tree milestone

Added GUI NTFS files with subfolders and CLI image ntfs-tree. Resolve safe Unicode parent paths
with directory/in-use/base-record and sequence checks; bound depth to 32, directory count to 1023
and paths to 4095 bytes. Reject cyclic/stale/missing/unsafe ancestors and preserve completed outputs.
NTFS integration tests cover nested Unicode paths through service/CLI and malformed ancestors.
Allocation/index verification, extensions and deleted files are the next milestones.

## NTFS extensions, consistency and streaming milestone

Added GUI/CLI checked NTFS recovery with MFT/volume bitmap checks, parent directory index membership,
resident index roots and bounded INDX traversal with fixups/VCNs/bitmap/cycle checks. Resolve bounded
resident/nonresident user-file ATTRIBUTE_LIST references with exact sequence/base/type/name/instance
checks and contiguous multi-record VCN coverage. MFT bootstrap lists/nested lists remain unsupported.
Safe exporter pulls and verifies 64 KiB chunks; checked files <=2 GiB, initialized MFT <=128 MiB.
Metadata streams <=16 MiB, attribute lists <=1 MiB, <=64 extension records/file and <=4096 runs/file.
Tests include split DATA extensions, malformed references, allocation/index mismatches, INDX corruption,
17 MiB streamed output/cancellation and 20 MiB MFT snapshots with explicit resource limits.

## Deleted candidates milestone

Added GUI/CLI ntfs-deleted and fat-deleted. NTFS requires unused MFT records and currently free data
clusters, retained valid attributes and sequence-matching parents/extensions; deleted parent directories
may resolve retained paths. Protect live/system metadata, named streams and earlier output extents.
FAT scans live directory trees for deleted short entries and infers only contiguous free, unclaimed
clusters, reserving all reachable live chains and conservative live spans before candidates. Output
names explicitly replace the lost first short-name character; deleted LFNs/directories are unsupported.
Confidence text warns that free clusters and retained metadata do not establish original contents.
New image commands install cooperative SIGINT/SIGTERM handlers. Final validation follows below.

Final local validation for the three milestones: Debug 28/28 (6.20 s), Release 28/28 (2.07 s),
ASan/UBSan image/FAT metadata/FAT files/NTFS integration 4/4 (27.63 s; Qt leak detection disabled).
Selected NTFS tree/checking/streaming/deleted tests exit normally under GDB: 34 QtTest passes
including setup/cleanup. GUI clicks and CLI subprocesses cover checked/deleted modes. Source bytes,
no-overwrite, partial-output cleanup and cancellation are verified on synthetic fixtures.
Graphify: 3971 nodes / 7240 edges; 20 existing config-node source_file warnings. Stage-two GitHub
Linux Debug and Release passed for 36909e2 (run 41225845937). Final head needs its own CI check.
No Windows or real-hardware validation, release completion or merge is claimed.

## Stage 4 — Persistent recovery sessions

Added optional versioned JSON sessions across image export methods, full-source SHA-256 identity,
pending/completed per-output hashes, atomic state replacement and single-process locking. Resume
replays discovery, verifies earlier files through anchored handles and retries remaining items without
overwrite. CLI image --session and session resume; GUI session path and File/Resume configuration
review. Tests cover interruption/restart, CLI, source/output tampering, malformed schema/engine,
configuration mismatch, pending/missing outputs and concurrent locks. Hard-killed incomplete output
is reported as a conflict; no arbitrary file is deleted. Linux integration validation follows.

## Stage 5 — Native read-only device acquisition and Windows I/O

Added native Linux sysfs and Windows storage-interface enumeration with measured geometry and
reported identity fields. Removed manufactured storage health/NAND/vendor-command results.
PhysicalDisk now delegates to a read-only native device reader; all physical writes are disabled.
CLI `device capture <device> --output <new-image>` streams and verifies an exclusive image output,
reports SHA-256 and removes its incomplete output on cancellation/errors. Mounted source disks on
Linux and online source disks on Windows are rejected. Output on the same underlying physical
disk is rejected. No device state is changed automatically. Use normal image recovery afterwards.

Windows image I/O uses Unicode native paths, retained file IDs and write/delete sharing restrictions.
Windows export now anchors each component to retained directory handles, rejects reparse points and
unsafe/reserved names, creates outputs exclusively, verifies bytes and atomically replaces only the
selected session state. The Windows CI job compiles and runs all registered tests; actual CI outcome
will be recorded separately. Native I/O tests cover Unicode, source identity, output paths, state
replacement, no-overwrite, cancellation, regular-file/device rejection and injectable Linux sysfs.
Local Debug: 30 registered test executables. Real hardware remains untested.

## Stage 6 — Development packaging and parser fuzzing

Added CPack runtime-only Linux TGZ/Ubuntu DEB and Windows portable ZIP generation, package SHA-256
files, desktop integration and release-QA instructions. Windows bundles Qt via windeployqt; the
official VC redistributable is a documented prerequisite. CI uploads development artifacts only.
A shared synthetic corpus feeds installed/extracted package smoke tests (FAT/NTFS exact bytes,
carving, Unicode source/output paths, resume, no-overwrite, source preservation and GUI startup).
Clang libFuzzer instruments library code with ASan/UBSan and exercises carving plus FAT/NTFS
metadata/file/tree/checked/deleted parsers. CI includes a bounded fuzz run and fresh read-only
Linux loop-device acquisition with mounted-source rejection and output comparison.

Local TGZ generation and extracted-package CLI/GUI smoke passed. No local Debian packaging tools
or passwordless loop-device permissions are available; those checks run on dedicated CI fixtures.
Windows first CI caught storage-header include ordering, fixed at 6206532. Further CI validation
and fuzz/sanitizer totals are recorded in the final checkpoint. Real hardware, signing, application
license decision/third-party notice review and code review remain stable-release gates.

Local stage 4–6 validation: Debug 30/30 (6.27 s), Release 30/30 (2.03 s), six ASan/UBSan
integration executables 6/6 (29.04 s; Qt leak detection disabled). The final optimized Clang harness,
including MBR/GPT, completed 20,000 fuzz runs in 32 s (625/s; peak RSS 445 MiB), with fatal
sanitizer recovery disabled and no findings. The earlier exploratory Debug harness is separate.
Linux CI at 2b716c2 passed all tests and generated TGZ/DEB packages with installed/extracted smoke;
its loop capture produced a verified 32 MiB image, then its unprivileged comparison failed on the
root-owned 0600 output. The fixture comparison now uses sudo. Windows SDK duplication was narrowed
to the user-mode winioctl header. A fresh head must pass all jobs before merge.

At 87fa53c GitHub Linux Debug/Release and fuzz passed (run 41234730249), including TGZ/DEB
installed/extracted CLI+GUI smoke and the 32 MiB read-only loop fixture. Windows compiled all
targets and passed 27/30 test executables; three Windows fixture/checkpoint tests need fixes.
Boot-analysis fixtures now use separate images instead of rewriting a source held read-only.
Windows QtTest reports are captured to files and printed in CI, and destination handles request
add/rename rights only at the selected output directory. No safety checks or tests are disabled.

## 2026-11-04 — CI-only validation and prototype delivery

User requested continued Linux and Windows work and no testing on their workstation. Keep
application, package, fuzz and device tests on disposable CI runners from this checkpoint.
Local source edits, Graphify updates and Git saves/pushes remain authorized.

Added persistent CTest JUnit/Qt reports for both platforms, including failed runs, and a
combined prototype job gated on Linux Debug/Release, Windows Release, fuzz and bundle integrity
checks. It validates all three package SHA-256 sidecars before producing source-revision metadata,
installation notes and the combined artifact. No public release, tag or merge is performed.

Static review found that Windows checkpoint cleanup deleted the newly committed journal if
its post-rename flush failed. Preserve the committed state on that error; delete only an
uncommitted temporary file. Existing checkpoint tests remain enabled in Windows CI.

No tests were run on the user's workstation for this checkpoint. CI from fc11f11 remains
unverified here: the GitHub connection is missing and unauthenticated API access returns 404.
A GitHub connection has been requested; do not claim Windows or prototype qualification without
reading the resulting exact-head checks.
