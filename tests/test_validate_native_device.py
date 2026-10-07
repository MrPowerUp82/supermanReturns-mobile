import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('collector', Path(__file__).resolve().parents[1] / 'tools/validate_native_device.py')
collector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collector)


class Clock:
    def __init__(self): self.now = 0
    def monotonic(self): return self.now
    def sleep(self, seconds): self.now += seconds


class Logs:
    def terminate(self): pass
    def wait(self, timeout): return 0


class Adb:
    def __init__(self, clock, restart=False, stopped=False, quiet=False):
        self.clock, self.restart, self.stopped, self.quiet = clock, restart, stopped, quiet
    def pid(self):
        return None if self.stopped else ('222' if self.restart and self.clock.now else '111')
    def start_logs(self, path, pid):
        if self.quiet:
            path.write_text('')
            return Logs()
        path.write_text('100.0 I renderer=pc-native-vulkan frame=120 hooks=9 packets=50 draws=20\n'
                        '102.0 I renderer=pc-native-vulkan frame=180 hooks=19 packets=90 draws=40\n')
        return Logs()
    def memory(self, pid): return 'TOTAL PSS: 12345\n Graphics: 6789\n'
    def exit_info(self): return 'No exit records'
    def boot_identity(self, pid): return 'renderer=pc-native-vulkan source=abc digest=def shader_sha256=123'
    def apk_hash(self): return '456'


class CollectorTests(unittest.TestCase):
    def collect(self, **kwargs):
        with tempfile.TemporaryDirectory() as temporary:
            clock = Clock()
            result = collector.collect(Adb(clock, **kwargs), 2, Path(temporary), clock=clock)
            saved = json.loads((Path(temporary) / 'summary.json').read_text())
            self.assertEqual(result, saved)
            return result
    def test_boot_identity_is_restricted_to_the_current_process(self):
        adb = collector.Adb('adb', 'device', 'org.supermanreturns.mobile.native')
        calls = []
        def run(*args, **kwargs):
            calls.append(args)
            return 'renderer=pc-native-vulkan source=current digest=new shader_sha256=123'
        adb.run = run
        self.assertIn('source=current', adb.boot_identity('222'))
        self.assertEqual(calls, [('logcat', '--pid=222', '-d', '-v', 'epoch')])
    def test_stopped_process_cannot_pass(self):
        result = self.collect(stopped=True)
        self.assertFalse(result['measurement_complete'])
        self.assertEqual(result['status'], 'not_running')
    def test_pid_change_interrupts_without_merging_runs(self):
        result = self.collect(restart=True)
        self.assertFalse(result['measurement_complete'])
        self.assertEqual(result['status'], 'interrupted')
        self.assertEqual(result['pid'], '111')
        self.assertEqual(result['replacement_pid'], '222')
    def test_completed_measurement_is_not_gameplay_approval(self):
        result = self.collect()
        self.assertTrue(result['measurement_complete'])
        self.assertFalse(result['gameplay_verified'])
        self.assertEqual(result['fps'], 30.0)
        self.assertEqual(result['memory_samples'][0]['pss_kb'], 12345)
        self.assertEqual(result['memory_samples'][0]['graphics_kb'], 6789)
    def test_process_without_native_frames_cannot_pass(self):
        result = self.collect(quiet=True)
        self.assertFalse(result['measurement_complete'])
        self.assertEqual(result['status'], 'no_native_frames')


if __name__ == '__main__': unittest.main()
