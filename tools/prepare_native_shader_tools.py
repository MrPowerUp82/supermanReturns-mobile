"""Prepare traceable host shader tools; never edit the PC checkout."""
import argparse
import json
from pathlib import Path
import shutil
import tempfile
import uuid
from prepare_native_renderer import digest, git, selected_sources, tree_hashes, remove_private_tree

ROOT = Path(__file__).resolve().parents[1]
COMMON = Path('build/vulkan-m2/emitter-tree/src/XenosRecomp/shader_common.h')


def prepare(source, output, patches_dir, helper, verify=False):
    source, output, helper = source.resolve(strict=True), output.resolve(), helper.resolve(strict=True)
    if source == output or source.is_relative_to(output) or output.is_relative_to(source) or output == output.parent:
        raise ValueError('Source and output must be separate, non-root directories')
    inputs = selected_sources(source, {'include': ['tools/shaders/*.py', COMMON.as_posix()]})
    if COMMON.as_posix() not in inputs:
        raise ValueError('PC common shader header is missing')
    patches = sorted(patches_dir.glob('*.patch'))
    metadata = {'format_version': 1, 'abi': 'sr-vulkan-buffers-v2',
                'commit': git(source, 'rev-parse', 'HEAD').decode().strip(),
                'inputs': {name: digest(path) for name, path in inputs.items()},
                'helper_sha256': digest(helper),
                'patches': [{'name': p.name, 'sha256': digest(p)} for p in patches]}
    changed = git(source, 'diff', '--name-only', '-z', 'HEAD', '--').split(b'\0')
    changed += git(source, 'ls-files', '--others', '--exclude-standard', '-z').split(b'\0')
    metadata['dirty_files'] = sorted({p.decode() for p in changed if p.decode() in inputs})
    if verify:
        lock = json.loads((output / 'source-lock.json').read_text(encoding='utf-8'))
        if any(lock.get(k) != v for k, v in metadata.items()):
            raise ValueError('Shader tools snapshot is stale')
        if lock['outputs'] != tree_hashes(output):
            raise ValueError('Shader tools snapshot was modified after preparation')
        print('Verified private shader tools v2')
        return
    output.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix=f'.{output.name}.stage-', dir=output.parent))
    backup = output.with_name(f'.{output.name}.backup-{uuid.uuid4().hex}')
    try:
        for name, path in inputs.items():
            target = stage / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(path.read_bytes())
        git(stage, 'init', '-q')
        for patch in patches:
            transport = stage / '.git/transport.patch'
            transport.write_bytes(patch.read_bytes().replace(b'\r\n', b'\n'))
            git(stage, 'apply', '--ignore-whitespace', '--check', str(transport.resolve()))
            git(stage, 'apply', '--ignore-whitespace', str(transport.resolve()))
        shutil.rmtree(stage / '.git')
        common = stage / COMMON
        common.write_bytes(helper.read_bytes() + b'\n' + common.read_bytes())
        if metadata['inputs'] != {name: digest(path) for name, path in inputs.items()} or metadata['helper_sha256'] != digest(helper):
            raise ValueError('Inputs changed during shader tool preparation')
        metadata['outputs'] = tree_hashes(stage)
        (stage / 'source-lock.json').write_text(json.dumps(metadata, indent=2, sort_keys=True) + '\n', encoding='utf-8')
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
        print(f'Prepared {len(inputs)} private shader tool inputs; ABI v2')
    finally:
        if stage.exists():
            remove_private_tree(stage, output.parent, f'.{output.name}.stage-')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recomp', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / '.tools/native-shader-tools')
    parser.add_argument('--patches', type=Path, default=ROOT / 'tools/shader-patches')
    parser.add_argument('--helper', type=Path, default=ROOT / 'native/shaders/float_filter.hlsl')
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    try:
        prepare(args.recomp, args.output, args.patches, args.helper, args.verify)
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        parser.exit(1, f'Shader tools preparation failed: {error}\n')


if __name__ == '__main__':
    main()
