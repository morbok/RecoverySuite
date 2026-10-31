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
    rm -rf -- "$fixture"
}
trap cleanup EXIT
truncate -s 32M "$fixture/source.img"
mkfs.ext4 -q -F "$fixture/source.img"
mkdir "$fixture/mount" "$fixture/output"
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
cmp "$fixture/source.img" "$fixture/output/captured.img"
if sudo "$cli" device capture "$loop_device" --output "$fixture/output/captured.img"; then
    echo 'Existing destination was incorrectly accepted' >&2; exit 1
fi
cmp "$fixture/source.img" "$fixture/output/captured.img"
echo 'Read-only loop acquisition passed: mounted-source rejection, exact bytes, no-overwrite'
