"""Check tracked/staged files and credentials without printing secret values."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--staged', action='store_true', help='Check only the pending commit')
    args = parser.parse_args()
    root = Path(subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], text=True).strip())
    command = ['git', 'diff', '--cached', '--name-only', '--diff-filter=ACMR', '-z'] if args.staged else ['git', 'ls-files', '-z']
    paths = subprocess.check_output(command, cwd=root).decode().split('\0')
    blocked = []
    for name in filter(None, paths):
        path = Path(name)
        private = any(part in {'.claude', '.codex', '.ssh', '.aws', '.azure', '.kube'} for part in path.parts)
        private |= path.name.startswith('.env') and path.name not in {'.env.example', '.env.sample', '.env.template'}
        private |= path.suffix.lower() in {'.pem', '.key', '.p12', '.pfx', '.jks', '.keystore'}
        private |= path.name in {'id_rsa', 'id_ed25519', 'credentials.json', 'service-account.json', '.netrc', '.npmrc', '.pypirc'}
        if private:
            blocked.append(name)
    if blocked:
        print('Private configuration/credential files cannot be committed:')
        for name in blocked:
            print(name)
        return 1
    scanner = os.environ.get('GITLEAKS_BIN') or shutil.which('gitleaks')
    if not scanner:
        scanner = subprocess.run(['git', 'config', '--path', 'recoverysuite.gitleaksPath'], cwd=root, capture_output=True, text=True).stdout.strip()
    if not scanner:
        print('Gitleaks is required. See CONTRIBUTING.md; credential check did not run.')
        return 2
    with tempfile.TemporaryDirectory(prefix='recoverysuite-secret-check-') as directory:
        report = Path(directory) / 'redacted.json'
        command = [scanner, 'git', str(root), '--redact=100', '--no-banner', '--ignore-gitleaks-allow', '--report-format=json', '--report-path='+str(report)]
        command += ['--pre-commit', '--staged'] if args.staged else ['--log-opts=--all --full-history']
        try:
            result = subprocess.run(command, cwd=root, capture_output=True, text=True)
        except OSError:
            print('Credential scanner unavailable; check blocked without printing configuration.')
            return 2
        if result.returncode:
            findings = json.loads(report.read_text()) if report.exists() else []
            for item in findings:
                print(f"Credential candidate: {item.get('File')}:{item.get('StartLine')} ({item.get('RuleID')})")
            print('Credential check blocked the commit.' if findings else 'Credential scanner failed; no output values were printed.')
            return 1 if findings else 2
    print('Private-file and credential checks passed (' + ('staged changes' if args.staged else 'all available Git history') + ').')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
