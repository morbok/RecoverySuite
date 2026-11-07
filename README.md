<p align="center">
  <img src="packaging/recoverysuite.svg" width="72" alt="RecoverySuite">
</p>

# RecoverySuite

**Find lost files. Review what survived. Recover them to another drive.**

RecoverySuite is a native **Windows and Linux disk data recovery application** for accidentally deleted files, lost partitions, and data made inaccessible after an interrupted write or disconnected drive. Recoverable filesystem records can contain documents, videos, archives, photos, code, or other file types.

**Status: unsigned development prototype.** Automated tests pass on disposable Windows/Linux fixtures. Real failing-drive behavior and physical USB/SATA/NVMe recovery still need separate hardware validation. Recovery is limited by surviving metadata and readable bytes; full Disk Drill feature parity is not claimed.

[Get the prototype](#download-and-install) · [How to recover files](#recover-files-in-five-steps) · [Supported recovery](#what-can-it-recover) · [User guide](docs/USER_GUIDE.md) · [Contribute](CONTRIBUTING.md)

## Download and install

The [Build and test workflow](https://github.com/morbok/RecoverySuite/actions/workflows/build.yml) produces a combined **RecoverySuite-prototype** artifact only after all its required jobs pass.

1. Open a successful run and scroll to **Artifacts**.
2. Download **RecoverySuite-prototype** and extract it. GitHub may require sign-in.
3. Read `START_HERE.md`. Check the package against `SHA256SUMS` or `manifest.json`; the manifest identifies the exact source commit and CI run.

[Verified MIT-licensed bundle — e0d468f](https://github.com/morbok/RecoverySuite/actions/runs/42255314931/artifacts/13521908107) · [Its validation run](https://github.com/morbok/RecoverySuite/actions/runs/42255314931)

CI artifacts expire. If that download is unavailable, use a newer successful run. There is currently no signed stable release.

| Platform | Package | Open the application |
| --- | --- | --- |
| Windows x64 | ZIP inside `Windows/` | Extract the package, then open `bin/RecoverySuite_GUI.exe`. Keep the bundled DLLs/plugins beside it. |
| Ubuntu 24.04 | DEB inside `Linux/` | On a separate evaluation machine, install the package with `sudo apt install ./Linux/RecoverySuite-*.deb`, then launch `RecoverySuite_GUI`. |
| Linux archive | TGZ inside `Linux/` | Extract it, then open `usr/bin/RecoverySuite_GUI`. Requires the Ubuntu 24.04 build distribution's Qt6 and libudev runtime libraries. |

Windows requires the official [Microsoft Visual C++ 2015–2022 x64 Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).
Linux requires Qt6 Core/Gui/Widgets/DBus and libudev; desktop authorization uses UDisks2/polkit. Other Linux distributions are not yet qualified.

## Recover files in five steps

**Before starting:** stop using the affected drive. Prepare a destination on a **different physical disk**. RecoverySuite opens sources read-only; it does not repair or format them. An active system disk is not a supported source.

1. **Select the affected disk.** In **Drives**, match its capacity and device path. Linux volumes must be unmounted; Windows disks must already be offline. RecoverySuite does not change this state.
2. **Open it for recovery.** Choose **Use selected drive**. If needed, choose **Authorize read-only access…** on Linux or **Open administrator window…** on Windows.
3. **Search.** Choose **Search for lost data**. Leave the deep signature search enabled for additional JPEG/PDF/ZIP candidates, or disable it to inspect filesystem records sooner.
4. **Review.** Filter by filename, folder or extension. Highlight a result and choose **Inspect highlighted file…** for a bounded thumbnail or text/hex preview. Check the files you want.
5. **Recover.** Choose an existing folder on another disk, then **Recover selected files**. Existing output files are preserved. Saved bytes are read back and verified.

You can also use **Open an existing disk image instead…**. A disk image is optional; it is a sector-by-sector source file, not a restriction to recovering pictures.

A successful copy verifies the exported bytes. It does **not** prove that deleted data had never been overwritten. A preview alone does not prove that an entire file is intact.

### Pause or continue later

Choose **Cancel** to pause a scan, then **File → Save scan results…** to save a `.rscan` catalog on another disk. Reopen it using **File → Open saved scan…**, then **File → Continue interrupted scan**. Keep the original source available at the same path. A catalog stores results and progress, not the recovered file contents.

### If the disk has read errors

Use **File → Rescue drive to image (logs unreadable sectors)…** to copy readable sectors to another disk. Keep the image and its `.rescue.json` report together. **File → Continue interrupted rescue…** resumes verified checkpoints.

Unreadable sectors become explicitly reported zero-filled gaps. Rescue cannot recreate missing data or guarantee a timeout when the operating system stalls. See the [rescue guide](docs/USER_GUIDE.md#rescue-a-disk-with-read-errors) before use.

## What can it recover?

Filesystem recovery works with general file contents, regardless of extension. Deep signature search has a narrower list of supported formats.

| Source / method | Implemented behavior | Main limits |
| --- | --- | --- |
| FAT12/16/32 | Supported existing/deleted file candidates and folder reconstruction | Deleted chains are uncertain; bounded cluster analysis, automatic streaming up to 2 GiB |
| exFAT 1.0 | Single-FAT files/folders, retained deleted entries, checked backup boot fallback | No TexFAT; surviving valid geometry/allocation metadata required |
| NTFS | Bounded supported MFT/file reconstruction, including supported deleted candidates | No encrypted/compressed streams or unsupported attribute layouts |
| ext4 | Retained allocated files, extent trees, sparse ranges and checked indexed directories | No deleted-inode recovery or journal replay |
| Lost partitions | Discover recognizable surviving filesystem metadata without partition entries | Cannot reconstruct every destroyed boot region/superblock |
| Signature search | JPEG, PDF and ZIP candidates | Up to 16 MiB per candidate; arbitrary fragmented carving is unsupported |

Current scan bounds include **10,000 candidates**, **128 volumes**, and **up to 2 GiB per supported streamed file**, with smaller parser-specific limits. Scan details explain skipped or partial records.

Overwritten, unwritten, or SSD TRIM-discarded bytes cannot be recreated. Unsupported filesystems, physical failures, encrypted/compressed content and heavily damaged metadata can prevent recovery. See [product scope](docs/PRODUCT_SCOPE.md) for exact boundaries.

## Troubleshooting

| Message or situation | Next step |
| --- | --- |
| Source mounted / busy / online | Prepare the disk outside RecoverySuite, then refresh. Do not use the active OS disk. |
| Access denied | Use the authorization action. On Linux, check UDisks2 and the desktop authentication agent. |
| Destination rejected / file already exists | Choose another physical disk and a new output name. Existing data is preserved. |
| Saved scan rejected | Reconnect the same source at the same path, or start a new scan. |
| No candidates / partial results | Read scan details. Metadata may be unsupported, overwritten or unreadable. A rescue image can preserve readable sectors. |

The [complete user guide](docs/USER_GUIDE.md) covers scanning, previews, saved catalogs, rescue, and error handling.

## Build and validation

For source builds, see [contributing](CONTRIBUTING.md). CMake 3.20+, C++20 and Qt6 are required.

CI runs Windows Release, Linux Debug/Release, 31 CTest executables per configuration, bounded ASan/UBSan parser fuzzing, package checks, and owned disposable loop/VHD/device-error fixtures. The combined prototype is produced only after its required build jobs pass. A separate **Credential safety** workflow scans available Git history for suspected secrets.

Synthetic CI success does not establish physical-hardware qualification. See [release acceptance](docs/RELEASE_ACCEPTANCE.md) and [release QA](docs/RELEASE_QA.md).

## Repository guide

| Location | Purpose |
| --- | --- |
| `src/` | Application, GUI/CLI, disk access and recovery implementation |
| `include/` | Public partition interfaces |
| `tests/` | Automated tests and parser fuzz harness |
| `scripts/` | CI packaging, disposable-device fixtures and credential checks |
| `cmake/`, `packaging/` | Build packaging and desktop assets |
| `docs/` | [Documentation index](docs/README.md), user guide, scope and development records |
| `docs/archive/` | Historical milestone notes; not current instructions |
| `.github/`, `.githooks/` | CI workflows and opt-in contributor commit protection |

## Privacy, security and license

**No API keys, cloud accounts or paid services are required for recovery.** Keep credentials, private configuration, disk images, recovered files, scan catalogs and rescue reports outside Git. Never attach real disk data or credentials to public issues. See [security guidance](SECURITY.md).

RecoverySuite's own code is [MIT licensed](LICENSE). Bundled dependencies retain their respective licenses and [notices](docs/THIRD_PARTY_NOTICES.md).
