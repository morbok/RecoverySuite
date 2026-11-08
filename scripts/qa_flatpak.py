"""Exercise only the CI-installed Flatpak using owned synthetic files."""
from pathlib import Path
from tempfile import TemporaryDirectory
import hashlib
import os
import subprocess
import time
from fuzz_corpus import fixtures


if os.environ.get('CI') != 'true' or os.environ.get('GITHUB_ACTIONS') != 'true':
    raise SystemExit('Flatpak application checks run only on disposable GitHub runners')

APP = 'io.github.morbok.RecoverySuite'


def cli(*args, expected=0):
    result = subprocess.run(['flatpak', 'run', '--user', '--command=RecoverySuite_CLI', APP,
                             *map(str, args)], capture_output=True, timeout=60)
    if result.returncode != expected:
        raise RuntimeError(f'Flatpak CLI exit {result.returncode}: {result.stderr.decode(errors="replace")}')
    return result


# /tmp is private inside Flatpak. Use the runner's home-mounted workspace instead.
with TemporaryDirectory(prefix='recoverysuite-flatpak-', dir=os.environ['RUNNER_TEMP']) as directory:
    root = Path(directory)
    for name, content in fixtures().items():
        source = root / name
        source.write_bytes(content)
        before = hashlib.sha256(content).digest()
        output = root / (name + '-out')
        output.mkdir()
        if name == 'signature.img':
            cli('image', 'carve', source, '--output', output)
            assert (output / 'carved_0.jpg').read_bytes() == bytes.fromhex('ffd8ff68656c6c6fffd9')
        else:
            method = 'files' if name.startswith('fat') else 'ntfs-files'
            cli('image', method, source, '--output', output)
            recovered = output / 'HELLO.TXT'
            assert recovered.read_bytes() == b'hello'
            digest = hashlib.sha256(recovered.read_bytes()).digest()
            duplicate = subprocess.run(['flatpak', 'run', '--user', '--command=RecoverySuite_CLI', APP,
                                        'image', method, str(source), '--output', str(output)],
                                       capture_output=True, timeout=60)
            assert duplicate.returncode != 0, 'Flatpak accepted an existing output'
            assert hashlib.sha256(recovered.read_bytes()).digest() == digest
        assert hashlib.sha256(source.read_bytes()).digest() == before
print('Installed Flatpak passed: FAT/NTFS exact bytes, signature recovery, source preservation and no-overwrite')

# No real disk or desktop interaction: load GUI/plugins using Qt's offscreen backend.
process = subprocess.Popen(['flatpak', 'run', '--user', '--env=QT_QPA_PLATFORM=offscreen', APP],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
try:
    time.sleep(2)
    if process.poll() is not None:
        out, err = process.communicate()
        raise RuntimeError(f'Flatpak GUI startup failed: {err.decode(errors="replace")}')
    print('Installed Flatpak GUI startup passed (offscreen)')
finally:
    subprocess.run(['flatpak', 'kill', '--user', APP], capture_output=True)
    process.communicate(timeout=15)
