"""Verify Android ARM64 packaging, 16 KB ELF alignment, and absence of retail inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("apk",type=Path)
parser.add_argument("--with-game",action="store_true",help="Require the recompiled guest and Android runtime libraries")
args=parser.parse_args()
with zipfile.ZipFile(args.apk) as archive:
    names=archive.namelist()
    assert not any(Path(n).suffix.lower() in {".xex",".iso",".ast",".xexp"} for n in names), "Retail input in APK"
    libraries=[n for n in names if n.startswith("lib/") and n.endswith(".so")]
    allowed={"lib/arm64-v8a/libc++_shared.so","lib/arm64-v8a/libsuperman_mobile.so"}
    if args.with_game:
        game={"lib/arm64-v8a/librexruntime.so","lib/arm64-v8a/libsuperman_game.so"}
        allowed|=game
        assert game<=set(libraries), "Missing game/runtime library"
        assert "assets/runtime-notices.txt" in names, "Missing runtime dependency notices"
    assert "lib/arm64-v8a/libsuperman_mobile.so" in libraries and set(libraries)<=allowed, libraries
    for name in libraries:
        data=archive.read(name)
        assert data[:6]==b'\x7fELF\x02\x01', "Expected ELF64 little endian"
        assert struct.unpack_from('<H',data,18)[0]==183, "Expected AArch64 machine"
        offset=struct.unpack_from('<Q',data,32)[0]
        size,count=struct.unpack_from('<HH',data,54)
        for i in range(count):
            p=offset+i*size
            if struct.unpack_from('<I',data,p)[0]==1:
                alignment=struct.unpack_from('<Q',data,p+48)[0]
                assert alignment>=16384, (name,alignment)
    assert "assets/NDK-libcxx-NOTICE.txt" in names, "Missing native dependency notice"
print(json.dumps({"apk":str(args.apk.resolve()),"bytes":args.apk.stat().st_size,
    "sha256":hashlib.sha256(args.apk.read_bytes()).hexdigest(),"abi":"arm64-v8a",
    "elf_page_alignment":"16 KB or greater","retail_inputs":False,"game_runtime":args.with_game},indent=2))
