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

## Current state

- GUI and CLI compile on Linux; all 23 registered CTest executables pass locally in Debug and Release.
- Recovery results distinguish completion, failure, partial recovery and cancellation.
- Unreported byte counts and elapsed time remain unknown; results show backend item names and sizes.
- Output export is **not implemented** in the GUI service. Selecting an output path does not save files.
- GUI startup currently has no disk reader; device discovery and selecting/opening a real source still need wiring.
- Backend calls remain synchronous inside the worker: cancellation is checked before and after a call,
  and cannot yet interrupt the scan itself.
- Windows builds and recovery against real devices have not been validated in this handoff.

See [the active roadmap](docs/MASTER_TODO.md), [session state](docs/SESSION_STATE.md),
and [the handoff](docs/HANDOFF.md) before continuing development.
