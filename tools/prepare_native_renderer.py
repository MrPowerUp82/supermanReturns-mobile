"""Snapshot selected PC native renderer sources without modifying the PC checkout."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import uuid

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git(directory, *args):
    # Git must not rewrite source bytes according to the developer's global settings.
    result = subprocess.run(['git', '-c', 'core.autocrlf=false', '-C', str(directory), *args], capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr.decode('utf-8', errors='replace').strip())
    return result.stdout


def selected_sources(source, manifest):
    selected = {}
    forbidden = {'.ast', '.xex', '.iso', '.srvk', '.srsl', '.dxil', '.log'}
    for pattern in manifest['include']:
        if Path(pattern).is_absolute() or '..' in Path(pattern).parts:
            raise ValueError(f'Unsafe source pattern: {pattern}')
        matches = source.glob(pattern + '/*' if pattern.endswith('/**') else pattern)
        for path in matches:
            if path.is_symlink() or not path.resolve().is_relative_to(source):
                raise ValueError(f'Source escapes checkout: {path}')
            if path.is_file():
                if path.suffix.lower() in forbidden:
                    raise ValueError(f'Private game data is not a renderer source: {path}')
                selected[path.relative_to(source).as_posix()] = path
    if not selected:
        raise ValueError('Source manifest selected no files')
    return dict(sorted(selected.items()))


def tree_hashes(directory):
    return {p.relative_to(directory).as_posix(): digest(p)
            for p in sorted(directory.rglob('*'))
            if p.is_file() and p.name != 'source-lock.json' and '.git' not in p.parts}


def remove_private_tree(path, parent, prefix):
    resolved = path.resolve()
    if resolved.parent != parent.resolve() or not resolved.name.startswith(prefix):
        raise ValueError(f'Refusing to remove unexpected private path: {resolved}')
    shutil.rmtree(resolved)


def prepare(source, output, manifest_path, patches_dir, verify=False):
    source, output = source.resolve(strict=True), output.resolve()
    if (source == output or source.is_relative_to(output) or output.is_relative_to(source)
            or output == output.parent):
        raise ValueError('Source and output must be separate, non-root directories')
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    sources = selected_sources(source, manifest)
    inputs = {name: digest(path) for name, path in sources.items()}
    commit = git(source, 'rev-parse', 'HEAD').decode().strip()
    changed = git(source, 'diff', '--name-only', '-z', 'HEAD', '--').split(b'\0')
    untracked = git(source, 'ls-files', '--others', '--exclude-standard', '-z').split(b'\0')
    dirty = sorted({p.decode('utf-8') for p in changed + untracked if p.decode('utf-8') in inputs})
    patch_paths = sorted(patches_dir.glob('*.patch'))
    patches = [{'name': path.name, 'sha256': digest(path)} for path in patch_paths]
    metadata = {'format_version': 1, 'commit': commit, 'inputs': inputs,
                'dirty_files': dirty, 'patches': patches}
    if verify:
        lock = json.loads((output / 'source-lock.json').read_text(encoding='utf-8'))
        if any(lock.get(key) != value for key, value in metadata.items()):
            raise ValueError('Source snapshot is stale: inputs, revision or patches changed')
        if lock['outputs'] != tree_hashes(output):
            raise ValueError('Source snapshot was modified after preparation')
        print(f'Verified {len(inputs)} native source files from {commit[:12]}')
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=f'.{output.name}.stage-', dir=output.parent))
    backup = output.with_name(f'.{output.name}.backup-{uuid.uuid4().hex}')
    try:
        for name, path in sources.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(path.read_bytes())
        if patch_paths:
            git(stage, 'init', '-q')
            for patch in patch_paths:
                # Patch transport line endings are not source file line endings.
                transport = stage / '.git/native-transport.patch'
                transport.write_bytes(patch.read_bytes().replace(b'\r\n', b'\n'))
                git(stage, 'apply', '--ignore-whitespace', '--check', str(transport.resolve()))
                git(stage, 'apply', '--ignore-whitespace', str(transport.resolve()))
            # Never leave a nested repository in the source snapshot.
            gitdir = stage / '.git'
            if gitdir.resolve().parent != stage.resolve():
                raise ValueError('Unexpected staging Git directory')
            shutil.rmtree(gitdir)
        # Detect concurrent edits before publishing an internally inconsistent snapshot.
        if inputs != {name: digest(path) for name, path in sources.items()}:
            raise ValueError('PC sources changed while preparing snapshot; retry')
        metadata['outputs'] = tree_hashes(stage)
        (stage / 'source-lock.json').write_text(
            json.dumps(metadata, indent=2, sort_keys=True) + '\n', encoding='utf-8')
        if output.exists():
            output.rename(backup)
        try:
            stage.rename(output)
        except OSError:
            if backup.exists():
                backup.rename(output)
            raise
        if backup.exists():
            remove_private_tree(backup, output.parent, f'.{output.name}.backup-')
        print(f'Prepared {len(inputs)} native source files, {len(patches)} patches; PC {commit[:12]}')
    finally:
        if stage.exists():
            remove_private_tree(stage, output.parent, f'.{output.name}.stage-')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recomp', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / '.tools/pc-native')
    parser.add_argument('--manifest', type=Path, default=ROOT / 'tools/native-source-manifest.json')
    parser.add_argument('--patches', type=Path, default=ROOT / 'tools/native-patches')
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    try:
        prepare(args.recomp, args.output, args.manifest, args.patches, args.verify)
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        parser.exit(1, f'Native source preparation failed: {error}\n')


if __name__ == '__main__':
    main()
