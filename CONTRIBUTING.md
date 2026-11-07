# Contributing to RecoverySuite

Start with the [README](README.md), [documentation index](docs/README.md) and [product scope](docs/PRODUCT_SCOPE.md). Current behavior is defined by supported recovery cases and exact-head CI evidence, not by archived completion notes.

## Keep credentials and private disk data out of Git

RecoverySuite itself requires no API keys. Use a private environment or credential manager for unrelated tooling. Only sanitized placeholder configuration may be shared.

Install [Gitleaks](https://github.com/gitleaks/gitleaks#installing), Python 3 and Git. The CI scanner is pinned to Gitleaks 8.30.1. Enable this checkout's commit check:

```sh
git config core.hooksPath .githooks
python3 scripts/check_secrets.py
```

The hook checks staged files before each commit, rejects private configuration filenames, and runs Gitleaks with redacted findings. It fails if the scanner is unavailable. If Gitleaks is outside PATH, configure its absolute path with `git config recoverysuite.gitleaksPath /path/to/gitleaks`. Windows contributors need Python 3 available as `python3` in Git's hook environment, or invoke the staged check with their Python command before committing.

The CI **Credential safety** workflow checks all available Git history. Findings show locations and rule names only; no reports containing credentials are uploaded. Do not bypass a finding or add a broad allowlist. Investigate privately. Detection is a useful safeguard, not a guarantee; see [SECURITY.md](SECURITY.md).

Do not commit local assistant settings, environment files, private keys, disk images, recovered contents or real scan/rescue reports. Git ignores are not a substitute for reviewing the staged diff.

## Build from source

Use CI or a separate evaluation machine for application and device tests. Do not run recovery against a workstation's real disks as a test.

### Ubuntu 24.04

Dependencies: CMake 3.20+, Ninja, a C++20 compiler, `qt6-base-dev`, and `libudev-dev`. CI device fixtures additionally need `exfatprogs`, `udisks2` and `dmsetup`. The dependency-install script is CI-only.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --timeout 60
```

### Windows x64

Use Visual Studio 2022 C++ tools, CMake and Qt 6.8.3 MSVC 2022 x64. Make Qt available through `CMAKE_PREFIX_PATH` and its `bin` directory on PATH.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure --timeout 60
```

For distribution packages, use CI: Windows packaging also needs collected upstream Qt notices. [Release QA](docs/RELEASE_QA.md) documents install/extract checks and packaging.

## Submit a change

- Use a feature branch and a focused commit; open or update a pull request.
- Describe the user-visible change, supported limits and relevant validation.
- Keep source access read-only, exports exclusive and destination guards intact.
- Add meaningful regression coverage for changes to recovery or safety behavior.
- Let required CI finish on the exact commit. Independent review is required before merge.
- Update current documentation when behavior changes. Historical notes belong under `docs/archive/`.

See [development rules](docs/DEVELOPMENT_RULES.md) for the repository's detailed conventions.
