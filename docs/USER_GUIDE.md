# Recover lost data with RecoverySuite

RecoverySuite finds supported files on connected disks after deletion, lost partitions, or an interrupted write. Use a destination on another physical disk.

## Start a scan

1. Connect the affected disk and choose it in **Drives**. Match its capacity and device path.
2. Its volumes must be unmounted on Linux or the disk offline on Windows. RecoverySuite does not change this state. An active system disk cannot be scanned.
3. Choose **Use selected drive**. If access is denied, use **Authorize read-only access…** on Linux or **Open administrator window…** on Windows. Linux needs UDisks2 and a desktop authentication agent; Windows displays the standard UAC prompt. Cancelled authorization leaves the source closed.
4. Choose **Search for lost data**. Leave deep search enabled to also search recognizable JPEG/PDF/ZIP signatures; disable it for filesystem records only.

An existing raw disk image is an optional source under **Open an existing disk image instead…**. Images use the same scan and recovery flow.

## Review and recover

Filter by name, folder or extension. Highlight a candidate and choose **Inspect highlighted file…**. Small supported raster images show a thumbnail; other data shows a bounded text/hex prefix. A preview does not prove the complete file is intact. No external viewer or active document content is opened.

Check the files you want, choose an existing folder on another disk, then choose **Recover selected files**. Existing output files are never overwritten. Saved files are read back and compared with the bytes reconstructed during recovery. This verifies the copy, not that deleted data was never overwritten.

## Pause, save and continue a scan

Choose **Cancel** to pause. Found candidates remain available for review/recovery. Use **File → Save scan results…** to save a new `.rscan` file on another disk. This saves the catalog and continuation point, not file contents.

After restarting, use **File → Open saved scan…** and **File → Continue interrupted scan**. The same source must be present at the same path. Identity, geometry and sampled bytes are checked. Completed volume scans are skipped; an interrupted filesystem volume is replayed with duplicate candidates suppressed. Deep search resumes from its earliest pending candidate (at most 16 MiB is reread). Unsaved progress is lost if the process crashes; save after pausing.

Saved catalogs from older versions can be reopened. They do not contain a continuation point and are treated as completed catalogs.

## Rescue a disk with read errors

Select the source, then **File → Rescue drive to image (logs unreadable sectors)…**. Choose a new `.img` on another disk. Failed 64 KiB reads fall back to single sectors with two retries. Unreadable sectors are filled with zeros and listed in `<image>.rescue.json`. Missing bytes remain missing data.

The image and report are flushed at checkpoints (normally every 64 MiB). Cancellation or failure keeps the partial image and report. To continue, select the same source, choose **File → Continue interrupted rescue…**, and select its report. The source binding, output file identity and saved prefix SHA-256 are checked before continuing. An uncommitted tail is discarded. Changed/tampered files are rejected. Reports made before resumable rescue was introduced cannot be continued.

Keep the image and report together. A final verification reads the output, without rereading failing source media. Open the completed image as an optional source and scan for files. Gaps are reported as partial results. Software cannot impose a timeout on an OS disk read that never returns or repair physically failing hardware.

CLI example: `RecoverySuite_CLI device rescue /dev/sdX --output /other-disk/rescue.img`; add `--resume` to continue. Exit 0 means no gaps, 3 means zero-filled gaps, and other nonzero exits mean failure/cancellation.

## Supported recovery and limits

Filesystem recovery works with general file contents, regardless of extension. It supports bounded FAT12/16/32 and NTFS reconstruction, single-FAT exFAT 1.0, and retained allocated ext4 files. Lost partitions require surviving recognizable filesystem metadata. Whole-source signature search supports JPEG/PDF/ZIP candidates only; a signature match is not a validity check.

Current bounds include 10,000 candidates, 128 volumes, up to 2 GiB for supported streamed files, and smaller per-parser metadata/buffer limits. FAT buffered recovery and signature candidates are limited to 16 MiB. See scan details for skipped or unsupported records.

Encrypted/compressed streams, arbitrary fragmented deleted chains, exFAT TexFAT, ext4 deleted records/journal replay/checksummed indexed directories and other filesystem families are unsupported. The app never writes source sectors, repairs partitions, formats disks, or restores discarded SSD TRIM data. Unwritten/overwritten bytes cannot be recreated.

## If something fails

- **Source busy / mounted / online:** stop using the source and prepare it outside RecoverySuite; refreshing the drive list does not unmount or take a disk offline.
- **Access not authorized:** cancel or retry using the access action. On Linux check that UDisks2 and your desktop authentication agent are installed. No disk-group membership or global permissions are changed.
- **Source changed / saved scan rejected:** reconnect the original source at the same path or start a new scan. Sample checks are not a full-disk comparison.
- **No candidates:** the filesystem/metadata may be unsupported, overwritten or unreadable. Review scan details. Rescue to an image can preserve what is still readable.
- **Output exists / destination rejected:** choose a new name or another folder on another physical disk. Existing files are preserved.
- **Partial result:** review per-file issues and rescue gap ranges. Do not treat a copied file as proof of complete original data.

This is an unsigned software prototype. Automated Windows/Linux checks use disposable fixtures. Physical USB/SATA/NVMe, real disconnect/failing-media behavior, independent review, application licensing and release signing remain external release gates.
