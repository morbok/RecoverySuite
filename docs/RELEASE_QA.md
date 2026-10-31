# Development release QA

This is a development build, not a stable recovery release. Packages are generated from the
reviewed build job; no automatic tag, public release or merge is performed.

## Packages

- Linux TGZ: extract and run `usr/bin/RecoverySuite_GUI` or `usr/bin/RecoverySuite_CLI`.
  Requires the Qt6 Core/Gui/Widgets libraries and libudev from the build distribution.
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
python3 scripts/package_smoke.py build/bin/RecoverySuite_CLI
(cd build && cpack -G TGZ)
# Debian/Ubuntu build host only:
(cd build && cpack -G DEB)
```

For Windows use `--config Release` and `ctest -C Release`; CPack `-C Release -G ZIP`.
Run the smoke script against the *extracted package* as well as the build tree. The script uses
only synthetic FAT/NTFS/signature fixtures, checks recovered bytes and session resume, and verifies
no-overwrite behavior. It does not touch real devices.

## Fuzzing

```sh
cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug -DRECOVERYSUITE_FUZZING=ON
cmake --build build-fuzz --target fuzz_recovery_parsers --parallel 2
python3 scripts/fuzz_corpus.py build-fuzz/corpus
ASAN_OPTIONS=detect_leaks=0 build-fuzz/bin/fuzz_recovery_parsers build-fuzz/corpus -runs=20000 -max_len=262144 -timeout=5
```

The harness exercises FAT files/trees/deleted candidates, NTFS MFT/root/tree/checked/deleted
parsers and signature carving with bounded in-memory sources. Valid synthetic seeds reach
metadata paths; mutations explore corruption. Expected validation exceptions are accepted;
ASan/UBSan failures, process crashes and timeouts fail the job. Fuzzing is bounded and is not
a proof that every malformed disk is safe.

## Hardware and release gate

CI loop-device acquisition tests operate only on a fresh synthetic read-only loop device.
Physical USB/SATA/NVMe disks, disconnects, permission policies, failing media and Windows VHD/device
acquisition still require dedicated QA before a stable release. Verify same-disk rejection,
source hashes, cancellation, no-overwrite and exact-byte output using disposable fixtures.
Do not select the OS/system disk as a source. Windows requires an already offline source;
Linux requires an unmounted source. Acquisition stops on read errors and offers no rescue retries.

SMART/NVMe diagnostics, hex inspection, plugins, broader damaged/deleted chain inference, OEM
short names and nested/MFT-bootstrap attribute lists are outside the implemented milestone.
Review current CI on the exact head and complete code review before merging.
