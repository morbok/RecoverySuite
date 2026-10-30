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
./build/bin/RecoverySuite_CLI image carve source.img --output /existing/empty-folder --start 0
```

Use `--sector-size 4096` for a 4 KiB-sector image, `--count N` to bound the scan, and
`--no-verify` to skip readback comparison. Ctrl+C or SIGTERM cancels cooperatively and keeps
completed outputs. Exit codes: 0 complete, 1 failed/partial, 2 invalid arguments, 130 cancelled.
Physical-device export and background operation tracking are unavailable; the legacy recovery
command returns an error instead of reporting unsaved buffers as recovered files.

## Current state

- GUI and CLI compile on Linux; all 25 registered CTest executables pass locally in Debug and Release.
- The GUI connects real image selection, range/output validation, worker scanning, safe file export
  and results. Progress and cancellation reach the streaming scan and output loops.
- POSIX output export rejects traversal, symlink components, unsafe names and existing files. It
  anchors the selected directory with a file descriptor; incomplete writes are removed on failure.
- The image workflow is carving-only. Legacy filesystem recovery/metadata APIs do not
  share this export workflow and need further implementation and validation.
- Windows safe export is unavailable; Windows builds and recovery against real devices remain unvalidated.
- Integration tests use synthetic images; the image workflow also passes AddressSanitizer/UBSan locally.

See [the active roadmap](docs/MASTER_TODO.md), [session state](docs/SESSION_STATE.md),
and [the handoff](docs/HANDOFF.md) before continuing development.
