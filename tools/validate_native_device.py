"""Collect one native-renderer process session; never certify visual gameplay."""
import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import subprocess
import time

PACKAGE = 'org.supermanreturns.mobile'


class Adb:
    def __init__(self, executable, serial, package=PACKAGE):
        self.command = [str(executable), '-s', serial]
        self.package = package

    def run(self, *args, missing_ok=False):
        result = subprocess.run(self.command + list(args), capture_output=True,
                                text=True, encoding='utf-8', errors='replace', timeout=30)
        if result.returncode and not (missing_ok and not result.stderr.strip()):
            raise RuntimeError(result.stderr.strip() or result.stdout.strip() or 'ADB command failed')
        return result.stdout

    def pid(self):
        value = self.run('shell', 'pidof', self.package + ':game', missing_ok=True).strip()
        if not value: return None
        if not value.isdigit(): raise RuntimeError('Expected exactly one game process: ' + value)
        return value

    def start_logs(self, path, pid):
        with path.open('wb') as output:
            return subprocess.Popen(self.command + ['logcat', '--pid=' + pid, '-v', 'epoch', '-T', '1'],
                                    stdout=output, stderr=subprocess.STDOUT)

    def memory(self, pid):
        return self.run('shell', 'dumpsys', 'meminfo', pid)

    def exit_info(self):
        return self.run('shell', 'dumpsys', 'activity', 'exit-info', self.package)

    def boot_identity(self, pid):
        # game.log is appended across boots; its first banner may describe an old APK.
        return self.run('logcat', '--pid=' + pid, '-d', '-v', 'epoch')

    def apk_hash(self):
        paths = self.run('shell', 'pm', 'path', self.package).splitlines()
        path = next((p.removeprefix('package:') for p in paths if p.endswith('/base.apk')), None)
        if not path or not path.startswith('/data/app/'):
            raise RuntimeError('Installed package APK path unavailable')
        return self.run('shell', 'sha256sum', path).split()[0]


def collect(adb, duration, output, clock=time, sample_interval=60):
    if duration <= 0: raise ValueError('Duration must be positive')
    output.mkdir(parents=True, exist_ok=True)
    summary = {'started_utc': datetime.now(timezone.utc).isoformat(), 'requested_seconds': duration,
               'duration_seconds': 0, 'pid': None, 'status': 'not_running',
               'measurement_complete': False, 'gameplay_verified': False,
               'memory_samples': [], 'fps': None,
               'visual_actions_required': ['city', 'movement', 'flight', 'landing', 'combat', 'pause', 'background_resume']}
    logs = None
    start = clock.monotonic()
    try:
        summary['pid'] = adb.pid()
        if not summary['pid']:
            summary['error'] = 'Game process is not running; no gameplay session was measured.'
        else:
            summary['apk_sha256'] = adb.apk_hash()
            identity = re.search(r'renderer=pc-native-vulkan source=(\S+) digest=(\S+) shader_sha256=(\S+)', adb.boot_identity(summary['pid']))
            if identity:
                summary.update(source_revision=identity[1], source_digest=identity[2], shader_sha256=identity[3])
            logs = adb.start_logs(output / 'native-process.log', summary['pid'])
            next_sample = 0
            while True:
                elapsed = clock.monotonic() - start
                current_pid = adb.pid()
                if current_pid != summary['pid']:
                    summary.update(status='interrupted', replacement_pid=current_pid,
                                   error='Game process exited or changed PID; sessions were not merged.')
                    break
                if hasattr(logs, 'poll') and logs.poll() is not None:
                    raise RuntimeError('Process log capture stopped unexpectedly')
                if elapsed >= next_sample or elapsed >= duration:
                    memory = adb.memory(current_pid)
                    (output / f'memory-{len(summary["memory_samples"]):03d}.txt').write_text(memory, encoding='utf-8')
                    pss = re.search(r'TOTAL PSS:\s*(\d+)', memory) or re.search(r'^\s*TOTAL\s+(\d+)', memory, re.M)
                    graphics = re.search(r'Graphics:\s*(\d+)', memory)
                    summary['memory_samples'].append({'seconds': round(elapsed, 3),
                        'pss_kb': int(pss[1]) if pss else None,
                        'graphics_kb': int(graphics[1]) if graphics else None})
                    next_sample = elapsed + sample_interval
                if elapsed >= duration:
                    summary.update(status='completed', measurement_complete=True)
                    break
                clock.sleep(min(5, duration - elapsed))
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        summary.update(status='error', error=str(error))
    finally:
        summary['duration_seconds'] = round(clock.monotonic() - start, 3)
        if logs:
            logs.terminate()
            try: logs.wait(timeout=5)
            except subprocess.TimeoutExpired:
                logs.kill()
                logs.wait(timeout=5)
        try:
            (output / 'exit-info.txt').write_text(adb.exit_info(), encoding='utf-8')
        except (OSError, RuntimeError, subprocess.SubprocessError) as error:
            summary['exit_info_error'] = str(error)
        path = output / 'native-process.log'
        if path.exists():
            text = path.read_text(encoding='utf-8', errors='replace')
            frames = [(float(t), int(f), int(d)) for t, f, d in re.findall(
                r'^(\d+(?:\.\d+)?)\s+.*renderer=pc-native-vulkan frame=(\d+).*?draws=(\d+)', text, re.M)]
            if len(frames) >= 2 and frames[-1][0] > frames[0][0] and frames[-1][1] >= frames[0][1]:
                summary['fps'] = round((frames[-1][1] - frames[0][1]) / (frames[-1][0] - frames[0][0]), 3)
                summary['draw_delta'] = frames[-1][2] - frames[0][2]
                summary['fps_source'] = 'Guest swap counters at log intervals; not display presentation timing'
                summary['average_swap_ms'] = round(1000 / summary['fps'], 3) if summary['fps'] else None
            identity = re.search(r'renderer=pc-native-vulkan source=(\S+) digest=(\S+) shader_sha256=(\S+)', text)
            if identity:
                summary.update(source_revision=identity[1], source_digest=identity[2], shader_sha256=identity[3])
        if summary['measurement_complete'] and (not summary.get('source_revision') or not summary['fps'] or summary.get('draw_delta', 0) <= 0):
            summary.update(status='no_native_frames', measurement_complete=False,
                           error='No native Vulkan session with advancing swap and draw counters was measured.')
        (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', type=Path, required=True)
    parser.add_argument('--device', required=True)
    parser.add_argument('--package', choices=[PACKAGE, PACKAGE + '.native'], default=PACKAGE)
    parser.add_argument('--duration', type=int, default=600)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    result = collect(Adb(args.adb, args.device, args.package), args.duration, args.output)
    print(json.dumps(result, indent=2))
    return 0 if result['measurement_complete'] else 1


if __name__ == '__main__': raise SystemExit(main())
