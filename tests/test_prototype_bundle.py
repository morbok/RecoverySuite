"""CI-only integrity regressions; never starts RecoverySuite or opens a device."""
from pathlib import Path
import hashlib
import importlib.util
import json
import tempfile
import unittest

source = Path(__file__).resolve().parents[1] / "scripts" / "assemble_prototype.py"
spec = importlib.util.spec_from_file_location("assemble_prototype", source)
bundle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bundle)
REVISION = "a" * 40
WORKFLOW = "https://github.com/example/RecoverySuite/actions/runs/123"


class PrototypeBundleTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.packages = []
        for platform, name in (("Linux", "RecoverySuite-dev.tar.gz"),
                               ("Linux", "RecoverySuite-dev.deb"),
                               ("Windows", "RecoverySuite-dev.zip")):
            path = self.root / platform / name
            path.parent.mkdir(exist_ok=True)
            path.write_bytes(b"synthetic package bytes " + name.encode())
            path.with_name(name + ".sha256").write_text(
                f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {name}\n")
            self.packages.append(path)

    def test_complete_bundle_preserves_inputs_and_records_source(self):
        before = {path: path.read_bytes() for path in self.packages}
        result = bundle.assemble(self.root, REVISION, WORKFLOW)
        self.assertEqual(result["sourceRevision"], REVISION)
        self.assertEqual({item["platform"] for item in result["packages"]}, {"Linux", "Windows"})
        self.assertEqual(len(result["packages"]), 3)
        self.assertEqual(json.loads((self.root / "manifest.json").read_text()), result)
        for path, content in before.items():
            self.assertEqual(path.read_bytes(), content)

    def test_incomplete_or_tampered_inputs_do_not_create_manifest(self):
        for fault in ("missing-platform", "missing-checksum", "tampered", "wrong-name", "duplicate"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as directory:
                import shutil
                root = Path(directory) / "bundle"
                shutil.copytree(self.root, root)
                path = root / "Windows" / self.packages[-1].name
                checksum = path.with_name(path.name + ".sha256")
                if fault == "missing-platform":
                    path.unlink()
                elif fault == "missing-checksum":
                    checksum.unlink()
                elif fault == "tampered":
                    path.write_bytes(b"different contents")
                elif fault == "wrong-name":
                    checksum.write_text(hashlib.sha256(path.read_bytes()).hexdigest() + "  other.zip\n")
                else:
                    path.with_name("another.zip").write_bytes(b"ambiguous second package")
                with self.assertRaises(ValueError):
                    bundle.assemble(root, REVISION, WORKFLOW)
                self.assertFalse((root / "manifest.json").exists())

    def test_unknown_revision_is_rejected(self):
        with self.assertRaises(ValueError):
            bundle.assemble(self.root, "unknown", WORKFLOW)
        self.assertFalse((self.root / "manifest.json").exists())


if __name__ == "__main__":
    unittest.main()
