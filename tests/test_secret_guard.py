"""CI-only synthetic repositories: blocking, redaction and historical leaks."""
import os
from pathlib import Path
import secrets
import subprocess
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/check_secrets.py'


class SecretGuardTests(unittest.TestCase):
    def setUp(self):
        if os.environ.get('CI') != 'true':
            self.skipTest('Execute synthetic guard tests only on CI')
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.git('init', '-q')
        self.git('config', 'user.name', 'CI fixture')
        self.git('config', 'user.email', 'fixture@example.invalid')
        self.git('config', 'core.hooksPath', str(self.root / 'no-hooks'))
        self.write('README.md', 'Synthetic fixture\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'fixture')

    def git(self, *args):
        return subprocess.run(['git', *args], cwd=self.root, check=True, capture_output=True)

    def write(self, name, value):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(value)

    def check_guard(self, *args, env=None):
        return subprocess.run(['python3', str(SCRIPT), *args], cwd=self.root,
                              env=env or os.environ.copy(), text=True, capture_output=True)

    def test_clean_history(self):
        result = self.check_guard()
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_forced_private_file_rejected(self):
        self.write('.env', 'NOT_A_REAL_CREDENTIAL=fixture\n')
        self.git('add', '.env')
        result = self.check_guard('--staged')
        self.assertEqual(result.returncode, 1)
        self.assertIn('.env', result.stdout)
        self.assertNotIn('NOT_A_REAL_CREDENTIAL', result.stdout)

    def test_staged_credential_is_blocked_without_echo(self):
        token = 'ghp_' + secrets.token_hex(20)
        self.write('configuration.txt', 'github_token=' + token + '\n')
        self.git('add', '.')
        result = self.check_guard('--staged')
        self.assertEqual(result.returncode, 1)
        self.assertIn('Credential candidate', result.stdout)
        self.assertNotIn(token, result.stdout + result.stderr)

    def test_deleted_credential_remains_detectable_in_history(self):
        token = 'ghp_' + secrets.token_hex(20)
        self.write('configuration.txt', 'github_token=' + token + '\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'synthetic incident')
        self.git('rm', 'configuration.txt')
        self.git('commit', '-qm', 'delete synthetic incident')
        result = self.check_guard()
        self.assertEqual(result.returncode, 1)
        self.assertNotIn(token, result.stdout + result.stderr)

    def test_missing_scanner_fails_closed(self):
        env = dict(os.environ, GITLEAKS_BIN=str(self.root / 'missing-scanner'))
        result = self.check_guard('--staged', env=env)
        self.assertNotEqual(result.returncode, 0)


if __name__ == '__main__':
    unittest.main()
