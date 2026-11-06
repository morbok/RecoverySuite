"""Assemble already-tested Linux/Windows CI packages without running applications."""
from pathlib import Path
import hashlib
import json
import os
import re
import sys


PACKAGE_TYPES = (
    ("Linux", "*.tar.gz", "archive"),
    ("Linux", "*.deb", "debian"),
    ("Windows", "*.zip", "portable"),
)


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def assemble(directory, source_revision, workflow_url):
    root = Path(directory)
    if not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", source_revision):
        raise ValueError("A complete source revision is required")
    if not workflow_url.startswith("https://github.com/") or "/actions/runs/" not in workflow_url:
        raise ValueError("A GitHub CI run URL is required")
    packages = []
    # Validate every input before writing the prototype manifest.
    for platform, pattern, kind in PACKAGE_TYPES:
        matches = list((root / platform).glob(pattern))
        if len(matches) != 1:
            raise ValueError(f"Expected exactly one {platform} {kind} package")
        package = matches[0]
        checksum = package.with_name(package.name + ".sha256")
        if package.is_symlink() or checksum.is_symlink() or not checksum.is_file():
            raise ValueError(f"Missing or unsafe package checksum: {package.name}")
        if not package.is_file() or not package.stat().st_size:
            raise ValueError(f"Empty or invalid package: {package.name}")
        # CPack writes '<hex>  <filename>'; accept its optional binary marker.
        match = re.fullmatch(r"([0-9a-fA-F]{64}) [ *]([^\r\n]+)\r?\n?", checksum.read_text())
        if not match or match[2] != package.name:
            raise ValueError(f"Invalid checksum record: {package.name}")
        actual = digest(package)
        if actual != match[1].lower():
            raise ValueError(f"Package checksum mismatch: {package.name}")
        packages.append({"platform": platform, "kind": kind,
                         "path": package.relative_to(root).as_posix(),
                         "bytes": package.stat().st_size, "sha256": actual})
    manifest = {
        "schema": 1,
        "channel": "prototype",
        "sourceRevision": source_revision,
        "workflow": workflow_url,
        "validationScope": "Synthetic CI fixtures; physical hardware remains unqualified",
        "packages": packages,
    }
    (root / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    checksums = "".join(f"{item['sha256']}  {item['path']}\n" for item in packages)
    (root / "SHA256SUMS").write_text(checksums, encoding="utf-8")
    (root / "START_HERE.md").write_text(
        "# RecoverySuite prototype\n\n"
        f"Source revision: `{source_revision}`\n\n"
        f"[CI run and validation reports]({workflow_url})\n\n"
        "This bundle is produced only after Linux Debug/Release, Windows Release,\n"
        "synthetic device/package checks, parser fuzzing and bundle integrity checks pass.\n"
        "It is an unsigned development prototype, not a stable hardware-qualified release.\n\n"
        "## Linux\n\n"
        "The DEB targets Ubuntu 24.04. Install on a separate evaluation machine using\n"
        "`sudo apt install ./Linux/RecoverySuite-*.deb`, then launch `RecoverySuite_GUI`.\n"
        "Alternatively extract the TGZ and launch `usr/bin/RecoverySuite_GUI` inside it.\n"
        "The archive requires Qt6 Core/Gui/Widgets/DBus and libudev from the build distribution;\n"
        "compatibility with other Linux distributions has not been established.\n\n"
        "## Windows x64\n\n"
        "Extract the ZIP and launch `bin/RecoverySuite_GUI.exe` inside the extracted folder.\n"
        "Qt and its collected upstream license/attribution notices are bundled. The official Microsoft Visual C++ 2015–2022 x64 Redistributable\n"
        "is a prerequisite: https://aka.ms/vs/17/release/vc_redist.x64.exe\n\n"
        "## First evaluation\n\n"
        "Select a connected, unmounted/offline drive and Search for lost data.\n"
        "Review candidates, select files, then Recover selected files to another drive.\n"
        "Opening a raw image is an optional source, not a required capture step.\n"
        "Advanced methods are optional. Selected file recovery always verifies saved bytes.\n"
        "Save paused or completed scans with File > Save scan results, reopen them and continue partial scans.\n"
        "For read errors, rescue to another disk; keep the image and .rescue.json report together to resume.\n"
        "See the packaged USER_GUIDE.md or the application Help menu for the complete workflow.\n"
        "Linux can request read-only access through UDisks2; Windows offers a UAC administrator window.\n"
        "Raster thumbnails and bounded text/hex inspection do not prove complete-file integrity.\n"
        "Deleted outputs are uncertain candidates, not guaranteed original contents.\n"
        "Use `SHA256SUMS` or `manifest.json` to verify package integrity before use.\n\n"
        "No application tests or device acquisition were run on the user's workstation\n"
        "as part of this prototype delivery. CI uses disposable synthetic fixtures.\n"
        "Physical USB/SATA/NVMe, failing-media QA, signing, application licensing and\n"
        "review remain release gates. No public release or automatic merge occurs.\n",
        encoding="utf-8",
    )
    return manifest


if __name__ == "__main__":
    if os.environ.get("CI") != "true":
        raise SystemExit("Prototype delivery runs in CI, not on the developer workstation")
    workflow = (f"{os.environ['GITHUB_SERVER_URL']}/{os.environ['GITHUB_REPOSITORY']}"
                f"/actions/runs/{os.environ['GITHUB_RUN_ID']}")
    result = assemble(sys.argv[1], os.environ["PROTOTYPE_SOURCE_REVISION"], workflow)
    print(f"Prototype assembled: {len(result['packages'])} verified Linux/Windows packages")
