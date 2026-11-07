# RecoverySuite handoff

Checkout: `<checkout>/RecoverySuite`; branch `feat/recovery-results-handoff`.
PR: https://github.com/morbok/RecoverySuite/pull/1 . Review before merge; no automatic release/tag.

Read RELEASE_ACCEPTANCE.md, USER_GUIDE.md and PRODUCT_SCOPE.md first. Earlier percentage and image-first phase notes are historical.

This is a general disk data-recovery software prototype for Windows/Linux. Connected drives are primary; raw images are optional. Automatic scan finds recognizable lost FAT/exFAT/NTFS/ext4 volumes, reconstructs bounded supported filesystem candidates, and optionally searches JPEG/PDF/ZIP signatures. Review/filter/inspect/check candidates, then recover selected files with anchored no-overwrite export and saved-byte verification to another physical disk.

Implemented continuation features:
- Completed/paused `.rscan` catalogs: source-bound, bounded transactional restore, discovery cursor, completed-volume skip, interrupted-volume replay with deduplication and bounded pending signature replay.
- Rescue capture: read-only, bounded sector retries, explicit zero-filled gaps, mandatory durable sidecar, output-identity/prefix-SHA256 validation and process-crash continuation. Plain capture still aborts on read errors.
- Worker raster thumbnails and bounded text/hex preview; no external viewers or active markup.
- Linux UDisks2 read-only authorization with independently checked descriptor handoff; Windows UAC administrator-window action. Unmounted/offline source state is never changed automatically.
- Embedded and packaged guide; pinned official Qt Base/SVG/Image Formats runtime notices, per-file notice hashes and installed/extracted package validation.

31 CTest executables, Linux Debug/Release, Windows Release, disposable loop/VHD fixtures, standard-format ext4/exFAT byte comparisons, process-crash/fault/tamper cases, parser ASan/UBSan fuzzing and combined-package integrity gates run in GitHub Actions. Check the exact source revision's run; manifest.json binds the resulting packages to it. Do not run builds/tests/app/device scans on the user's workstation. Artifact inspection/hashing and Graphify AST updates are permitted locally.

Software bounds and unsupported cases are in USER_GUIDE.md/PRODUCT_SCOPE.md. Physical hardware/disconnect/failing-media and interactive-consent qualification, independent review, application-license choice and signing remain external release gates. Never claim Disk Drill feature parity or guaranteed recovery based on CI. No partition/source writes or live system-drive recovery are implemented.

Graphify is installed: query `graphify-out/graph.json` before source searches/reads, then update after code edits. Keep ignored graph output and local agent configuration out of commits. Shell commands use RTK. The active finite completion checklist is RELEASE_ACCEPTANCE.md; avoid reviving obsolete broad milestone lists as new required work.
