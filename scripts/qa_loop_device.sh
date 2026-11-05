#!/usr/bin/env bash
# Dedicated CI fixture only: never selects an existing host device.
set -euo pipefail
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
truncate -s 32M "$fixture/source.img"
mkfs.ext4 -q -F "$fixture/source.img"
python3 - "$fixture/source.img" <<'PY'
import sys
with open(sys.argv[1], 'r+b') as disk:
    disk.seek(24 * 1024 * 1024)
    disk.write(bytes.fromhex('ffd8ff68656c6c6fffd9'))
PY
mkdir "$fixture/mount" "$fixture/output" "$fixture/recovered"
loop_device=$(sudo losetup --find --show --read-only "$fixture/source.img")
sudo mount -o ro,noload "$loop_device" "$fixture/mount"
mounted=1
if sudo "$cli" device capture "$loop_device" --output "$fixture/output/mounted.img"; then
    echo 'Mounted source was incorrectly accepted' >&2; exit 1
fi
test ! -e "$fixture/output/mounted.img"
sudo umount "$fixture/mount"
mounted=0
sudo "$cli" device capture "$loop_device" --output "$fixture/output/captured.img"
sudo cmp "$fixture/source.img" "$fixture/output/captured.img"
if sudo "$cli" device capture "$loop_device" --output "$fixture/output/captured.img"; then
    echo 'Existing destination was incorrectly accepted' >&2; exit 1
fi
sudo cmp "$fixture/source.img" "$fixture/output/captured.img"
echo 'Read-only loop acquisition passed: mounted-source rejection, exact bytes, no-overwrite'
sudo "$cli" device scan "$loop_device"
sudo "$cli" device recover "$loop_device" --output "$fixture/recovered" --select 'Signature matches/carved_0.jpg'
sudo python3 - "$fixture/recovered/Signature matches/carved_0.jpg" <<'PY'
from pathlib import Path
import sys
assert Path(sys.argv[1]).read_bytes() == bytes.fromhex('ffd8ff68656c6c6fffd9')
PY
sudo cmp "$fixture/source.img" "$fixture/output/captured.img"
echo 'Direct physical-source scan and selected-file recovery passed without source changes'
