"""Traceable private shader tooling and atomic failure behavior."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'tools/prepare_native_shader_tools.py'
COMMON = Path('build/vulkan-m2/emitter-tree/src/XenosRecomp/shader_common.h')


class ShaderToolsTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.pc, self.output = self.root / 'pc', self.root / 'prepared'
        self.pc.mkdir()
        self.common = self.pc / COMMON
        self.common.parent.mkdir(parents=True)
        self.common.write_text('// common fixture\n', encoding='utf-8')
        tool = self.pc / 'tools/shaders/vulkan_contract.py'
        tool.parent.mkdir(parents=True)
        tool.write_text("ABI='sr-vulkan-buffers-v1'\n", encoding='utf-8')
        self.helper = self.root / 'helper.hlsl'
        self.helper.write_text('// helper fixture\n', encoding='utf-8')
        self.patches = self.root / 'patches'
        self.patches.mkdir()
        (self.patches / 'v2.patch').write_text(
            'diff --git a/tools/shaders/vulkan_contract.py b/tools/shaders/vulkan_contract.py\n'
            '--- a/tools/shaders/vulkan_contract.py\n+++ b/tools/shaders/vulkan_contract.py\n'
            "@@ -1 +1 @@\n-ABI='sr-vulkan-buffers-v1'\n+ABI='sr-vulkan-buffers-v2'\n",
            encoding='utf-8')
        self.git('init', '-q')
        self.git('add', '.')
        self.git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid', 'commit', '-qm', 'fixture')

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.pc), *args], text=True).strip()

    def run_prepare(self, *args, ok=True):
        r = subprocess.run([sys.executable, str(SCRIPT), '--recomp', str(self.pc),
                            '--output', str(self.output), '--patches', str(self.patches),
                            '--helper', str(self.helper), *args], capture_output=True, text=True)
        self.assertEqual(r.returncode == 0, ok, r.stdout + r.stderr)
        return r

    def test_v2_tool_snapshot(self):
        self.run_prepare()
        self.assertEqual((self.output / 'tools/shaders/vulkan_contract.py').read_text(),
                         "ABI='sr-vulkan-buffers-v2'\n")
        self.assertIn('// helper fixture', (self.output / COMMON).read_text())
        lock = json.loads((self.output / 'source-lock.json').read_text())
        self.assertEqual(lock['commit'], self.git('rev-parse', 'HEAD'))
        self.assertEqual(lock['helper_sha256'], hashlib.sha256(self.helper.read_bytes()).hexdigest())
        self.assertEqual(self.common.read_text(), '// common fixture\n')
        self.run_prepare('--verify')

    def test_verify_detects_helper_change(self):
        self.run_prepare()
        self.helper.write_text('// changed helper\n')
        r = self.run_prepare('--verify', ok=False)
        self.assertIn('stale', r.stderr.lower())

    def test_incompatible_patch_preserves_previous_output(self):
        self.run_prepare()
        old = (self.output / COMMON).read_bytes()
        self.common.write_text('// changed PC file\n')
        (self.patches / 'bad.patch').write_text('not a patch\n')
        self.run_prepare(ok=False)
        self.assertEqual((self.output / COMMON).read_bytes(), old)

    def test_tampered_output_fails_verification(self):
        self.run_prepare()
        (self.output / COMMON).write_text('// tampered\n')
        self.run_prepare('--verify', ok=False)

    def test_real_pc_tools_use_v2_contract(self):
        pc = ROOT.parent / 'superman_returns_recomp'
        if not (pc / COMMON).is_file():
            self.skipTest('Local PC source unavailable')
        before = hashlib.sha256((pc / COMMON).read_bytes()).hexdigest()
        r = subprocess.run([sys.executable, str(SCRIPT), '--recomp', str(pc),
                            '--output', str(self.output), '--helper', str(self.helper)],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        r = subprocess.run([sys.executable, '-B', '-c',
                            'import vulkan_contract as v; assert v.ABI == "sr-vulkan-buffers-v2"; '
                            'assert v.build_contract()["version"] == 2'],
                           cwd=self.output / 'tools/shaders', capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(before, hashlib.sha256((pc / COMMON).read_bytes()).hexdigest())


if __name__ == '__main__':
    unittest.main()
