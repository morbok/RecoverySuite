"""Exercise an installed CLI with synthetic images, exact bytes and persistent resume."""
from pathlib import Path
from tempfile import TemporaryDirectory
import hashlib
import json
import subprocess
import sys
from fuzz_corpus import fixtures

cli = str(Path(sys.argv[1]).resolve())


def run(*args, expected=0):
    result = subprocess.run([cli, *map(str, args)], capture_output=True, timeout=30)
    if result.returncode != expected:
        raise RuntimeError(f'{args}: exit {result.returncode}\n{result.stdout!r}\n{result.stderr!r}')
    return result


with TemporaryDirectory(prefix='recoverysuite-package-') as name:
    root = Path(name) / "İmaj 📷"
    root.mkdir()
    for filename, data in fixtures().items():
        source = root / filename; source.write_bytes(data)
        digest = hashlib.sha256(data).digest()
        output = root / (filename + '-out'); output.mkdir()
        if filename == 'signature.img':
            session = root / 'session.json'
            run('image', 'carve', source, '--output', output, '--session', session)
            recovered = output / 'carved_0.jpg'
            assert recovered.read_bytes() == bytes.fromhex('ffd8ff68656c6c6fffd9')
            before = recovered.stat().st_mtime_ns
            run('session', 'resume', session)
            assert recovered.stat().st_mtime_ns == before
            assert json.loads(session.read_text())['status'] == 'completed'
        else:
            method = 'files' if filename.startswith('fat') else 'ntfs-files'
            run('image', method, source, '--output', output)
            assert (output / 'HELLO.TXT').read_bytes() == b'hello'
            result = subprocess.run([cli, 'image', method, str(source), '--output', str(output)], capture_output=True, timeout=30)
            assert result.returncode != 0, 'existing output was accepted'
            assert (output / 'HELLO.TXT').read_bytes() == b'hello'
        assert hashlib.sha256(source.read_bytes()).digest() == digest
print('Package smoke passed: FAT/NTFS exact bytes, carving, resume, source preservation, no-overwrite')

# A running GUI confirms that Qt libraries and the platform plugin load from the package.
import os
import time
gui = Path(cli).with_name('RecoverySuite_GUI.exe' if os.name == 'nt' else 'RecoverySuite_GUI')
env = dict(os.environ, QT_QPA_PLATFORM='offscreen')
process = subprocess.Popen([str(gui)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
try:
    time.sleep(1)
    if process.poll() is not None:
        stdout, stderr = process.communicate()
        raise RuntimeError(f'Packaged GUI exited during startup: {stdout!r} {stderr!r}')
finally:
    process.terminate()
    try:
        process.communicate(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill(); process.communicate()
print('Packaged GUI startup passed (offscreen Qt platform)')
