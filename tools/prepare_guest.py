"""Prepare a private SDK header overlay for an ARM64 codegen compile check.

Does not alter the desktop project, ship game code, or link a runnable runtime.
The two fixes extend the SDK's existing libc++ fallbacks to Android NDK r27.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--recomp", type=Path, required=True)
args = parser.parse_args()
source = args.recomp.resolve()
headers = source / ".tools/rexglue-sdk/win-amd64/include"
generated = source / "port/generated/default"
if not (headers / "rex/ppc.h").is_file() or not (generated / "sources.cmake").is_file():
    parser.error("Expected the local ReXGlue v0.10.0 SDK headers and generated Superman C++.")
overlay = root / ".tools/guest-sdk/include"
shutil.copytree(headers, overlay, dirs_exist_ok=True)
patches = {
    "rex/string/numeric.h": ("#if REX_PLATFORM_MAC", "#if REX_PLATFORM_MAC || REX_PLATFORM_ANDROID"),
    "rex/chrono/chrono.h": ("#ifdef __APPLE__", "#if defined(__APPLE__) || defined(__ANDROID__)"),
}
provenance = {}
for relative, (before, after) in patches.items():
    original = (headers / relative).read_bytes()
    text = original.decode("utf-8")
    if before not in text:
        raise SystemExit(f"SDK changed; review the Android fallback patch: {relative}")
    (overlay / relative).write_text(text.replace(before, after), encoding="utf-8")
    provenance[relative] = hashlib.sha256(original).hexdigest()
(root / ".tools/guest-sdk/provenance.json").write_text(json.dumps({"source":str(source),"sdk":"0.10.0","original_headers":provenance},indent=2),encoding="utf-8")
print(f"Private header overlay ready: {overlay}")
print(f"Generated source stays local: {generated}")
