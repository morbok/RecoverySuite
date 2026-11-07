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
mkfs.exfat "$fixture/source.img"
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
    --select 'exFAT_0/Existing/Documents/payload.bin' --select 'exFAT_0/Existing/notes.txt'
sudo cmp "$fixture/payload.bin" "$fixture/recovered/exFAT_0/Existing/Documents/payload.bin"
sudo cmp "$fixture/notes.txt" "$fixture/recovered/exFAT_0/Existing/notes.txt"
sha256sum --check "$fixture/before.sha256"
echo 'Standard-formatted exFAT device passed: nested binary/document recovery and unchanged source'

# Damage only this newly-created fixture's main boot region, then use the
# intact real mkfs.exfat backup. Recovery must not repair/write the source.
sudo losetup -d "$loop_device"
loop_device=""
python3 - "$fixture/source.img" <<'PYDAMAGE'
import sys
with open(sys.argv[1],'r+b') as image:
    image.seek(108)
    shift=image.read(1)[0]
    assert 9<=shift<=12
    image.seek(0)
    image.write(bytes(12*(1<<shift)))
PYDAMAGE
sha256sum "$fixture/source.img" > "$fixture/damaged-before.sha256"
mkdir "$fixture/recovered-backup"
loop_device=$(sudo losetup --find --show --read-only "$fixture/source.img")
sudo "$cli" device scan "$loop_device" --quick
sudo "$cli" device recover "$loop_device" --output "$fixture/recovered-backup" \
    --select 'exFAT_0/Existing/Documents/payload.bin' --select 'exFAT_0/Existing/notes.txt'
sudo cmp "$fixture/payload.bin" "$fixture/recovered-backup/exFAT_0/Existing/Documents/payload.bin"
sudo cmp "$fixture/notes.txt" "$fixture/recovered-backup/exFAT_0/Existing/notes.txt"
sha256sum --check "$fixture/damaged-before.sha256"
echo 'Standard-formatted exFAT backup-boot recovery passed with erased main region and unchanged source'
