# Development release QA

This is a development build, not a stable recovery release. Packages are generated from the
reviewed build job; no automatic tag, public release or merge is performed.

## Packages

- Linux TGZ: extract and run `usr/bin/RecoverySuite_GUI` or `usr/bin/RecoverySuite_CLI`.
  Requires the Qt6 Core/Gui/Widgets/DBus libraries and libudev from the build distribution.
- Ubuntu 24.04 DEB: install with `sudo apt install ./RecoverySuite-*.deb`; dependencies are
  determined from the built ELF binaries by CPack/dpkg-shlibdeps. Use the CI artifact for that OS.
- Windows x64 ZIP: extract and run `bin/RecoverySuite_GUI.exe` or `bin/RecoverySuite_CLI.exe`.
  Qt libraries/plugins are bundled with windeployqt. Install the official Microsoft Visual C++
  2015–2022 x64 Redistributable if needed: https://aka.ms/vs/17/release/vc_redist.x64.exe
- SHA256 files accompany generated packages. Verify before installation. Development packages
  are unsigned. Repository/application licensing and bundled third-party notices need an explicit
  release review before redistribution; no application license has been invented by packaging.

## Repeatable checks

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --timeout 60
cmake --install build --prefix /tmp/recoverysuite-stage --component Runtime
python3 scripts/package_smoke.py /tmp/recoverysuite-stage/bin/RecoverySuite_CLI
(cd build && cpack -G TGZ)
# Debian/Ubuntu build host only:
(cd build && cpack -G DEB)
```

For Windows, first collect notices in CI with `python scripts/collect_qt_notices.py build/runtime-notices`. Use `--config Release` and `ctest -C Release`; CPack `-C Release -G ZIP`.
Run the smoke script against the *extracted package* as well as the installed staging directory. The script uses
only synthetic FAT/NTFS/signature fixtures, checks recovered bytes and session resume, and verifies
no-overwrite behavior. It does not touch real devices.

## Fuzzing

```sh
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=RelWithDebInfo -DRECOVERYSUITE_FUZZING=ON
cmake --build build-fuzz --target fuzz_recovery_parsers --parallel 2
python3 scripts/fuzz_corpus.py build-fuzz/corpus
ASAN_OPTIONS=detect_leaks=0 build-fuzz/bin/fuzz_recovery_parsers build-fuzz/corpus -runs=20000 -max_len=262144 -timeout=5
```

The harness exercises MBR/GPT partition parsers, FAT files/trees/deleted candidates, NTFS MFT/root/tree/checked/deleted
parsers and signature carving with bounded in-memory sources. Valid synthetic seeds reach
metadata paths; mutations explore corruption. Expected validation exceptions are accepted;
ASan/UBSan failures, process crashes and timeouts fail the job. Fuzzing is bounded and is not
a proof that every malformed disk is safe.

## Hardware and release gate

CI loop-device acquisition tests operate only on a fresh synthetic read-only loop device.
Windows CI creates a fresh fixed VHD, attaches it read-only and checks online-source rejection,
offline capture, exact payload hashes and no-overwrite. It changes only this new fixture disk.
Physical USB/SATA/NVMe disks, disconnects, permission policies and failing media still require
dedicated QA before a stable release. Verify same-disk rejection,
source hashes, cancellation, no-overwrite and exact-byte output using disposable fixtures.
Do not select the OS/system disk as a source. Windows requires an already offline source;
Linux requires an unmounted source. Plain capture stops on read errors; explicit rescue retries sectors, logs zero-filled gaps and supports verified checkpoint continuation.

SMART/NVMe diagnostics, whole-disk hex editing, plugins, broader damaged/deleted chain inference, OEM
short names and nested/MFT-bootstrap attribute lists are outside the implemented milestone.
Review current CI on the exact head and complete code review before merging.

## Prototype artifact and CI-only execution

Application/device tests must run on CI runners or a dedicated evaluation machine, not on the
user's workstation. Commands above describe that evaluation environment. Linux and Windows
remain required targets. `RecoverySuite-prototype` is produced only after all platform, fuzz
and bundle-integrity jobs succeed. It contains both platform packages, `START_HERE.md`,
`manifest.json` with source revision/run provenance and a combined `SHA256SUMS`. The assembly
step verifies CPack sidecars and refuses missing, duplicate or altered packages.

A failed platform blocks the combined artifact. CTest JUnit output, Qt text reports and
`LastTest.log` are uploaded separately as diagnostics, even on failure. The workflow may be
started manually with `workflow_dispatch`; it neither runs local tests nor publishes a release.

## General-drive completion checks

See RELEASE_ACCEPTANCE.md and USER_GUIDE.md. CI covers paused drive-catalog continuation across service restarts, a real-process crash during rescue, tampered-prefix and native-output-identity rejection, bounded raster preview, Linux UDisks2 read-only descriptor transport on an owned loop device, and Windows UAC API compilation. Interactive desktop consent is an external qualification item. Installed/extracted packages must include guides, exclude SDK headers, and verify all upstream runtime-notice file hashes. Windows notices are collected from pinned official Qt Base/SVG/Image Formats 6.8.3 source archives. The combined manifest binds all packages to the source revision and workflow.
