#!/usr/bin/env bash
# GitHub's disposable runner only; never uses a pre-existing disk.
set -euo pipefail
if [[ "${CI:-}" != true || "${GITHUB_ACTIONS:-}" != true ]]; then
    echo 'This fixture is restricted to disposable GitHub Actions runners' >&2; exit 1
fi
cli=$(realpath "$1")
fixture=$(mktemp -d)
loop_device=""
mounted=0
cleanup() {
    if [[ "$mounted" == 1 ]]; then sudo umount "$fixture/mount"; fi
    if [[ -n "$loop_device" ]]; then sudo losetup -d "$loop_device"; fi
    sudo rm -rf -- "$fixture"
}
trap cleanup EXIT
truncate -s 64M "$fixture/source.img"
mkfs.ext4 -q -F "$fixture/source.img"
mkdir "$fixture/mount" "$fixture/recovered"
loop_device=$(sudo losetup --find --show "$fixture/source.img")
sudo mount "$loop_device" "$fixture/mount"
mounted=1
python3 - "$fixture" <<'PYDATA'
from pathlib import Path
import sys
root = Path(sys.argv[1])
(root / 'payload.bin').write_bytes(bytes(range(256)) * 1024 + b'file tail')
(root / 'notes.txt').write_text('General disk recovery: document content.\n', encoding='utf-8')
PYDATA
sudo mkdir "$fixture/mount/Documents"
sudo cp "$fixture/payload.bin" "$fixture/mount/Documents/payload.bin"
sudo cp "$fixture/notes.txt" "$fixture/mount/notes.txt"
sync
sudo umount "$fixture/mount"
mounted=0
sudo losetup -d "$loop_device"
loop_device=""
sha256sum "$fixture/source.img" > "$fixture/before.sha256"
loop_device=$(sudo losetup --find --show --read-only "$fixture/source.img")
sudo "$cli" device scan "$loop_device"
sudo "$cli" device recover "$loop_device" --output "$fixture/recovered" \
    --select 'ext4_0/Existing/Documents/payload.bin' --select 'ext4_0/Existing/notes.txt'
sudo cmp "$fixture/payload.bin" "$fixture/recovered/ext4_0/Existing/Documents/payload.bin"
sudo cmp "$fixture/notes.txt" "$fixture/recovered/ext4_0/Existing/notes.txt"
sha256sum --check "$fixture/before.sha256"
echo 'Standard-formatted ext4 device passed: nested binary/document recovery and unchanged source'
