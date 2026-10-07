"""Behavioral tests for a reproducible, non-destructive PC source snapshot."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / 'tools/prepare_native_renderer.py'


class PrepareNativeRendererTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.pc = self.root / 'pc'
        self.pc.mkdir()
        self.source = self.pc / 'port/src/graphics/vulkan/example.cpp'
        self.source.parent.mkdir(parents=True)
        self.source.write_bytes(b'original\n')
        self.git('init', '-q')
        self.git('config', 'core.autocrlf', 'false')
        self.git('add', '.')
        self.git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                 'commit', '-qm', 'fixture')
        self.manifest = self.root / 'manifest.json'
        self.manifest.write_text(json.dumps({'include': ['port/src/graphics/vulkan/**']}))
        self.patches = self.root / 'patches'
        self.patches.mkdir()
        self.output = self.root / 'snapshot'

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.pc), *args], text=True).strip()

    def prepare(self, *extra, ok=True):
        result = subprocess.run([sys.executable, str(SCRIPT), '--recomp', str(self.pc),
                                 '--output', str(self.output), '--manifest', str(self.manifest),
                                 '--patches', str(self.patches), *extra],
                                capture_output=True, text=True)
        if ok:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
        return result

    def test_dirty_source_is_recorded(self):
        self.source.write_bytes(b'local edit\n')
        self.prepare()
        self.assertEqual((self.output / self.source.relative_to(self.pc)).read_bytes(), b'local edit\n')
        lock = json.loads((self.output / 'source-lock.json').read_text())
        self.assertEqual(lock['commit'], self.git('rev-parse', 'HEAD'))
        self.assertIn(self.source.relative_to(self.pc).as_posix(), lock['dirty_files'])
        self.assertEqual(lock['inputs'][self.source.relative_to(self.pc).as_posix()],
                         hashlib.sha256(b'local edit\n').hexdigest())

    def test_removed_source_does_not_survive(self):
        obsolete = self.source.with_name('obsolete.cpp')
        obsolete.write_bytes(b'obsolete')
        self.prepare()
        obsolete.unlink()
        self.prepare()
        self.assertFalse((self.output / obsolete.relative_to(self.pc)).exists())
        self.assertEqual(self.source.read_bytes(), b'original\n')

    def test_verify_detects_tamper(self):
        self.prepare()
        self.prepare('--verify')
        target = self.output / self.source.relative_to(self.pc)
        target.write_bytes(b'tamper')
        self.prepare('--verify', ok=False)
        self.assertEqual(target.read_bytes(), b'tamper')

    def test_incompatible_patch_keeps_previous_snapshot(self):
        self.prepare()
        previous = (self.output / 'source-lock.json').read_bytes()
        (self.patches / 'bad.patch').write_text(
            'diff --git a/missing.cpp b/missing.cpp\n--- a/missing.cpp\n+++ b/missing.cpp\n'
            '@@ -1 +1 @@\n-old\n+new\n')
        self.prepare(ok=False)
        self.assertEqual((self.output / 'source-lock.json').read_bytes(), previous)
        self.assertEqual((self.output / self.source.relative_to(self.pc)).read_bytes(), b'original\n')

    def test_source_and_output_must_not_overlap(self):
        result = subprocess.run([sys.executable, str(SCRIPT), '--recomp', str(self.pc),
                                 '--output', str(self.pc), '--manifest', str(self.manifest)],
                                capture_output=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(self.source.read_bytes(), b'original\n')

    def test_applied_patch_and_final_hash_are_recorded(self):
        (self.patches / 'edit.patch').write_bytes(
            b'diff --git a/port/src/graphics/vulkan/example.cpp b/port/src/graphics/vulkan/example.cpp\n'
            b'--- a/port/src/graphics/vulkan/example.cpp\n+++ b/port/src/graphics/vulkan/example.cpp\n'
            b'@@ -1 +1 @@\n-original\n+patched\n')
        self.prepare()
        self.assertEqual((self.output / self.source.relative_to(self.pc)).read_bytes(), b'patched\n')
        lock = json.loads((self.output / 'source-lock.json').read_text())
        self.assertEqual(lock['outputs'][self.source.relative_to(self.pc).as_posix()],
                         hashlib.sha256(b'patched\n').hexdigest())
        self.assertEqual(lock['patches'][0]['name'], 'edit.patch')
        self.assertEqual(self.source.read_bytes(), b'original\n')

    def test_verify_detects_source_drift(self):
        self.prepare()
        self.source.write_bytes(b'new upstream bytes\n')
        self.prepare('--verify', ok=False)

    def test_symlink_cannot_import_files_outside_source(self):
        external = self.root / 'outside.cpp'
        external.write_bytes(b'not a PC source')
        link = self.source.with_name('link.cpp')
        try:
            link.symlink_to(external)
        except OSError:
            self.skipTest('File symlinks require Windows developer mode')
        self.prepare(ok=False)


if __name__ == '__main__':
    unittest.main()
