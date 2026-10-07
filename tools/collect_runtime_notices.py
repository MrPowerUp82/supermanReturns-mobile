"""Bundle upstream license texts alongside the optional native runtime."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
sdk = root / '.references/rexglue-sdk'
sections = ['Native runtime: https://github.com/Buku313/rexglue-skate3-android\n'
            'Revision: edd4344723ecac3ffa18c5dcd2fcc268f468ff9e\n'
            'Dependency revisions are pinned by the upstream Git submodules.\n']
for directory in [sdk, *sorted((sdk / 'thirdparty').iterdir())]:
    if not directory.is_dir():
        continue
    for path in sorted(directory.iterdir()):
        if path.is_file() and path.name.upper().startswith(('LICENSE', 'LICENCE', 'COPYING', 'NOTICE')):
            sections.append(f'\n===== {path.relative_to(sdk).as_posix()} =====\n'
                            + path.read_text(encoding='utf-8', errors='replace'))
native_root = root / '.tools/pc-native'
native_lock = native_root / 'source-lock.json'
if native_lock.exists():
    import json
    lock = json.loads(native_lock.read_text(encoding='utf-8'))
    sections.append('\nPC native Vulkan renderer source revision: ' + lock['commit']
                    + '\nSource and adaptation hashes are recorded privately in source-lock.json.\n'
                    'Origin: crazyriddler/rexglue-native-kit @136bc6c4; '
                    'kit revision supplies no license file. Xenia/ReXGlue-derived '
                    'portions retain their BSD notices in source files.\n')
output = root / 'android/app/src/main/assets/runtime-notices.txt'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text('\n'.join(sections), encoding='utf-8')
print(f'{len(sections)-1} upstream notice files: {output}')
