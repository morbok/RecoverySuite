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
sudo mkdir "$fixture/mount/Indexed"
sudo python3 - "$fixture/mount/Indexed" <<'PYINDEX'
from pathlib import Path
import sys
root=Path(sys.argv[1])
for i in range(1000):
    (root / (f'entry-{i:04d}-'+('x'*150)+'.txt')).write_bytes(f'indexed content {i}\n'.encode())
PYINDEX
sync
sudo umount "$fixture/mount"
mounted=0
sudo losetup -d "$loop_device"
loop_device=""
# Force a real checksummed multi-level htree, rather than assuming mkdir indexed it.
if sudo e2fsck -f -D -y "$fixture/source.img"; then :; else test "$?" -eq 1; fi
sudo debugfs -R 'htree_dump /Indexed' "$fixture/source.img" > "$fixture/htree.txt"
python3 - "$fixture/htree.txt" <<'PYCHECK'
from pathlib import Path
import sys
assert 'Indirect levels: 1' in Path(sys.argv[1]).read_text(), 'Expected a real multi-level htree fixture'
PYCHECK
sha256sum "$fixture/source.img" > "$fixture/before.sha256"
loop_device=$(sudo losetup --find --show --read-only "$fixture/source.img")
sudo "$cli" device scan "$loop_device"
sudo "$cli" device recover "$loop_device" --output "$fixture/recovered" \
    --select 'ext4_0/Existing/Documents/payload.bin' --select 'ext4_0/Existing/notes.txt'
sudo cmp "$fixture/payload.bin" "$fixture/recovered/ext4_0/Existing/Documents/payload.bin"
sudo cmp "$fixture/notes.txt" "$fixture/recovered/ext4_0/Existing/notes.txt"
indexed_first="entry-0000-$(printf 'x%.0s' {1..150}).txt"
indexed_last="entry-0999-$(printf 'x%.0s' {1..150}).txt"
sudo "$cli" device recover "$loop_device" --output "$fixture/recovered" \
    --select "ext4_0/Existing/Indexed/$indexed_first" --select "ext4_0/Existing/Indexed/$indexed_last"
sudo python3 - "$fixture/recovered/ext4_0/Existing/Indexed" "$indexed_first" "$indexed_last" <<'PYVERIFY'
from pathlib import Path
import sys
root=Path(sys.argv[1])
assert (root/sys.argv[2]).read_bytes()==b'indexed content 0\n'
assert (root/sys.argv[3]).read_bytes()==b'indexed content 999\n'
PYVERIFY
sha256sum --check "$fixture/before.sha256"
echo 'Standard-formatted ext4 device passed: nested binary/document and multi-level indexed-directory recovery, unchanged source'
