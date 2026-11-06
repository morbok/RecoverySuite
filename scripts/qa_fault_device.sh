#!/usr/bin/env bash
# Kernel I/O faults on a newly owned disposable CI mapping, never an existing disk.
set -euo pipefail
[[ "${CI:-}" == true && "${GITHUB_ACTIONS:-}" == true ]] || exit 1
cli=$(realpath "$1")
fixture=$(mktemp -d)
loop_device=""
mapping="recoverysuite-${fixture##*.}"
created=0
cleanup() {
    if [[ "$created" == 1 ]]; then sudo dmsetup remove "$mapping"; fi
    if [[ -n "$loop_device" ]]; then sudo losetup -d "$loop_device"; fi
    sudo rm -rf -- "$fixture"
}
trap cleanup EXIT
python3 - "$fixture/source.img" <<'PYDATA'
import sys
with open(sys.argv[1],'wb') as output:
    for sector in range(65536):
        output.write(bytes([sector%251])*512)
PYDATA
sha256sum "$fixture/source.img" > "$fixture/before.sha256"
loop_device=$(sudo losetup --find --show --read-only "$fixture/source.img")
# Only the two sectors in this synthetic mapping return EIO. Everything else
# reads from the new read-only loop. No mapping is reloaded or taken offline.
table=$(printf '0 8 linear %s 0\n8 2 error\n10 65526 linear %s 10\n' "$loop_device" "$loop_device")
sudo dmsetup create "$mapping" --readonly --table "$table"
created=1
sudo udevadm settle
source_device=$(readlink -f "/dev/mapper/$mapping")
mkdir "$fixture/output"
if sudo "$cli" device capture "$source_device" --output "$fixture/output/plain.img"; then
    echo 'Plain capture incorrectly ignored kernel read errors' >&2; exit 1
fi
test ! -e "$fixture/output/plain.img"
set +e
sudo "$cli" device rescue "$source_device" --output "$fixture/output/rescue.img"
rescue_exit=$?
set -e
test "$rescue_exit" -eq 3
sudo python3 - "$fixture" <<'PYVERIFY'
from pathlib import Path
import hashlib,json,sys
root=Path(sys.argv[1]);expected=bytearray((root/'source.img').read_bytes())
expected[8*512:10*512]=bytes(2*512)
actual=(root/'output/rescue.img').read_bytes()
assert actual==expected
report=json.loads((root/'output/rescue.img.rescue.json').read_text())
assert report['status']=='completed-with-gaps' and report['storedBytesVerified'] is True
assert report['unreadableSectors']=='2'
assert report['sha256']==hashlib.sha256(actual).hexdigest()
PYVERIFY
sha256sum --check "$fixture/before.sha256"
echo 'Native kernel EIO rescue passed: exact zero-fill gaps, partial status, verification and unchanged source'
