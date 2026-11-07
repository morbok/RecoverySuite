"""Collect bounded notice files from the exact official Windows Qt runtime source."""
from pathlib import Path, PurePosixPath
import hashlib
import io
import json
import os
import sys
import tarfile
import urllib.request

VERSION = "6.8.3"
URL = f"https://download.qt.io/archive/qt/6.8/{VERSION}/submodules/qtbase-everywhere-src-{VERSION}.tar.xz"
SOURCES = {
    "qtbase": "56001b905601bb9023d399f3ba780d7fa940f3e4861e496a7c490331f49e0b80",
    "qtsvg": "35eb516460f00f264eb504baa253432384351cf23fb9980a5857190e8deef438",
    "qtimageformats": "049bfb99845e4801672aca07c3c4fc4c140f932a3a33faa899419579e33ef1c8",
}


def collect_module(destination, module, sha256):
    url=URL.replace("qtbase-",module+"-")
    request = urllib.request.Request(url, headers={"User-Agent": "RecoverySuite-CI-notice-collector"})
    with urllib.request.urlopen(request, timeout=90) as response:
        archive = response.read(128 * 1024 * 1024 + 1)
    if len(archive) > 128 * 1024 * 1024 or hashlib.sha256(archive).hexdigest() != sha256:
        raise ValueError("Qt source archive exceeds bounds or failed its pinned SHA-256")
    root = Path(destination)
    output = root / f"{module}-{VERSION}"
    records, total = [], 0
    with tarfile.open(fileobj=io.BytesIO(archive), mode="r:xz") as source:
        for member in source:
            parts = PurePosixPath(member.name).parts
            if not parts or parts[0] != f"{module}-everywhere-src-{VERSION}" or ".." in parts:
                raise ValueError("Unexpected Qt archive path")
            relative = PurePosixPath(*parts[1:])
            name = relative.name.lower()
            selected = (relative.parts and relative.parts[0] == "LICENSES") or (
                "3rdparty" in relative.parts and any(word in name for word in
                ("license", "copying", "copyright", "notice", "authors", "readme", "attribution")))
            if not selected or not member.isfile():
                continue
            total += member.size
            if member.size > 2 * 1024 * 1024 or total > 32 * 1024 * 1024 or len(records) >= 5000:
                raise ValueError("Qt notice inventory exceeds bounds")
            content = source.extractfile(member).read()
            path = output.joinpath(*relative.parts)
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
            records.append({"path": f"{module}-{VERSION}/" + relative.as_posix(), "bytes": len(content),
                            "sha256": hashlib.sha256(content).hexdigest()})
    if not records or (module == "qtbase" and not (output / "LICENSES" / "LGPL-3.0-only.txt").is_file()):
        raise ValueError("Required Qt license text is missing")
    return {"module": module, "source": url, "sourceSHA256": sha256, "notices": records}


def collect(destination):
    modules = [collect_module(destination, name, digest) for name, digest in SOURCES.items()]
    manifest = {"qtVersion": VERSION, "sources": [{k:v for k,v in m.items() if k != "notices"} for m in modules],
                "notices": [record for m in modules for record in m["notices"]]}
    (Path(destination) / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Collected {len(manifest['notices'])} Qt runtime notice files")



if __name__ == "__main__":
    if os.environ.get("CI") != "true":
        raise SystemExit("Runtime notice collection runs in CI")
    collect(sys.argv[1])
